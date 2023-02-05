#include "./master_server.h"
#include "core/tcpServer/tcpserver.h"
#include "modules/log/log.h"
#include "modules/meta_data/file.h"
#include "modules/meta_data/meta_data.h"
#include "modules/setting/setting.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <math.h>
#include <random>
#include <star.h>
#include <string>
#include <tuple>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace star
{
static io_lock::ptr server_lock( new io_lock() );

std::unordered_map< std::string, std::vector< std::tuple< std::string, size_t > > > chunk_server_meta_data; /* 元数据表，通过 chunk server 索引 */
std::unordered_map< std::string, master_server::chunk_server_info > chunk_server_list = {}; /* 所有的 chunk server 信息 */
std::unordered_map< std::string, master_server::chunk_server_info > fail_chunk_server = {}; /* 连接不上的chunk server */
std::unordered_map< std::string, master_server::chunk_server_info > available_chunk_server; /* 在线的chunk server */
std::vector< std::string > available_chunk_server_name = {}; /* 用来使用下标随机访问服务器的名 */

size_t master_server::copys          = 0;
size_t master_server::max_chunk_size = 0;
/* 打开这两个列表 */
levelDBList::ptr master_server::file_url_list = nullptr;

static size_t default_max_chunk_size = 0;

master_server::master_server( std::filesystem::path settings_path )
: tcpserver( settings_path )
{
    /* 配置服务器 */
    this->m_status = INIT;
    try
    {
        /* 读取chunk server 的信息 */
        Json::Value servers = this->m_settings->get( "chunk_server" );
        for ( int i = 0; i < servers.size(); i++ )
        {
            chunk_server_info cur;
            cur.addr                      = servers[i]["address"].asString();
            cur.port                      = servers[i]["port"].asInt64();
            chunk_server_list[cur.join()] = cur;
        }

        this->max_chunk_size   = this->m_settings->get( "Max_chunk_size" ).asInt();
        this->copys            = this->m_settings->get( "copy_num" ).asInt();
        default_max_chunk_size = this->max_chunk_size;

        this->print_logo();
        this->m_db->open();

        this->file_url_list.reset( new levelDBList( master_server::m_db, "file_url_list" ) );

        DEBUG_STD_STREAM_LOG( g_logger )
        << "File url list length: " << S( file_url_list->size() ) << Logger::endl();

        DEBUG_STD_STREAM_LOG( g_logger ) << "Init file name and path list Begin!" << Logger::endl();
        /* 初始化元数据表 */
        for ( size_t i = 0; i < file_url_list->size(); i++ )
        {
            DEBUG_STD_STREAM_LOG( g_logger )
            << "Get File url: " << file_url_list->get( i ) << Logger::endl();
            file::ptr new_file( new file( file_url_list->get( i ), this->max_chunk_size ) );
            new_file->open( file_operation::read, this->m_db );
            /* 初始化 chunk server meta data */
            for ( size_t k = 0; k < new_file->chunks_num(); k++ )
            {
                std::vector< std::string > arr;
                chunk::ptr cur_chunk( new chunk( new_file->get_name(), new_file->get_path(), k ) );
                new_file->read_chunk_meta_data( k, arr );
                chunk_server_meta_data[chunk_server_info::join( arr[0], std::stoi( arr[1] ) )]
                .push_back( std::tuple< std::string, size_t >( new_file->get_url(), k ) );
            }
            new_file->close();
        }
        DEBUG_STD_STREAM_LOG( g_logger ) << "Init file name and path list End!" << Logger::endl();

        /* 初始化租约管理器 */
        int32_t default_lease_time = this->m_settings->get( "default_lease_time" ).asInt();
        this->m_lease_control.reset( new lease_manager( default_lease_time ) );
    }
    catch ( std::exception e )
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "what: " << e.what() << Logger::endl();
        throw e.what();
    };
}

bool master_server::find_file_meta_data( std::vector< std::string >& res, std::string f_name, std::string f_path )
{
    /* 先使用副本替换失联的 chunk server 上的chunk */
    replace_unconnect_chunk();
    std::vector< std::string > arr;
    file::ptr cur_file( new file( f_name, f_path, this->max_chunk_size ) );
    cur_file->open( file_operation::read, this->m_db );

    if ( !cur_file->chunks_num() )
    {
        ERROR_STD_STREAM_LOG( g_logger )
        << "file: " << f_name << " find file failed！ %n what: File does not exist."
        << Logger::endl();
        return false;
    }

    for ( size_t i = 0; i < cur_file->chunks_num(); i++ )
    {
        cur_file->read_chunk_meta_data( i, arr );
        for ( auto item : arr )
        {
            res.push_back( item );
        }
    }
    cur_file->close();

    return true;
}

void master_server::replace_unconnect_chunk()
{
    server_lock->lock_write( file_operation::write );
    INFO_STD_STREAM_LOG( g_logger )
    << "%D"
    << "Chunk server connect error, replace chunk begin!" << Logger::endl();
    /* 遍历失联的服务器列表 */
    for ( auto item : fail_chunk_server )
    {
        /* 查找替换丢失的文件块元数据 */
        for ( auto iter : chunk_server_meta_data[item.first] )
        {
            chunk::ptr copy_file = nullptr;
            std::vector< std::string > res;
            /* 查看其副本是否在线 */
            for ( size_t i = 1; i < this->copys; i++ )
            {
                file::ptr origin_file( new file( std::get< 0 >( iter ), this->max_chunk_size ) );
                std::string copys_name = file::join_copy_name( origin_file->get_name(), i );
                copy_file.reset( new chunk( copys_name, origin_file->get_path(), std::get< 1 >( iter ) ) );
                copy_file->open( file_operation::read, this->m_db );
                copy_file->read_meta_data( res );
                copy_file->close();
                /* 判断副本所在的chunk server是否在线 */
                if ( fail_chunk_server[chunk_server_info::join( res[0], std::stoi( res[1] ) )]
                     .addr.empty() )
                {
                    break;
                }
            }
            if ( !copy_file || res.empty() )
            {
                /* 副本全部不在线 */
                ERROR_STD_STREAM_LOG( g_logger ) << Logger::endl();
                return;
            }
            /* 把 item 所在的 chunk server 标记为需要同步 */
            chunk_server_list[item.second.join()].is_need_sync = true;
            /* 用 copy file 的元数据来替换 iter 的元数据 */
            chunk::ptr need_replace( new chunk( std::get< 0 >( iter ), std::get< 1 >( iter ) ) );
            need_replace->open( file_operation::write, this->m_db );
            need_replace->read_meta_data( res );
            need_replace->close();
        }
    }
    INFO_STD_STREAM_LOG( g_logger )
    << "%D"
    << "Chunk server connect error, replace chunk End!" << Logger::endl();
    server_lock->release_write();
}

bool master_server::sync_chunk( chunk::ptr from, chunk::ptr dist )
{
    try
    {
        /* 向 chunk server 询问 from 的块数据 */
        protocol::Protocol_Struct prs(
        110, "", from->get_name(), from->get_path(), 0, "", { S( from->get_index() ) } );
        IPAddress::ptr addr = IPv4Address::Create( from->addr.c_str(), from->port );
        MSocket::ptr sock   = MSocket::CreateTCP( addr );
        if ( !sock->connect( addr ) )
        {
            FATAL_STD_STREAM_LOG( g_logger ) << "sync chunk: Ask chunk data Fail!" << Logger::endl();
            return false;
        }
        if ( tcpserver::send( sock, prs ) == -1 )
        {
            FATAL_STD_STREAM_LOG( g_logger ) << "sync chunk: Ask chunk data Fail!" << Logger::endl();
            return false;
        }
        protocol::ptr current_procotol = tcpserver::recv( sock, master_server::buffer_size );
        /* 看是否接受成功 */
        if ( !current_procotol )
        {
            FATAL_STD_STREAM_LOG( g_logger ) << "%D"
                                             << "Receive Message Error" << Logger::endl();
            server_lock->release_write();
            return false;
        }

        prs = current_procotol->get_protocol_struct();

        /* 判断回复是否正确 */
        if ( prs.bit == 115 )
        {
            /* 接受数据 */
            std::string data = prs.data;
            if ( data.size() != prs.package_size )
            {
                /* 数据不全 */
                ERROR_STD_STREAM_LOG( g_logger )
                << "Maybe the package is not Complete!" << Logger::endl();
                return false;
            }
            /* 把数据发给, dist 块所在的chunk server */
            prs.reset( 108, "", dist->get_name(), dist->get_path(), data.size(), data, {} );
            IPAddress::ptr addr = IPv4Address::Create( dist->addr.c_str(), dist->port );
            MSocket::ptr sock   = MSocket::CreateTCP( addr );
            if ( !sock->connect( addr ) )
            {
                FATAL_STD_STREAM_LOG( g_logger )
                << "sync chunk: Ask chunk data Fail!" << Logger::endl();
                return false;
            }
            tcpserver::send( sock, prs );
            protocol::ptr current_procotol = tcpserver::recv( sock, master_server::buffer_size );

            /* 看是否接受成功 */
            if ( !current_procotol )
            {
                FATAL_STD_STREAM_LOG( g_logger ) << "%D"
                                                 << "Receive Message Error" << Logger::endl();
                server_lock->release_write();
                return false;
            }

            prs = current_procotol->get_protocol_struct();

            /* 判断是否正确 */
            if ( prs.bit == 116 && prs.data == "true" )
            {
                INFO_STD_STREAM_LOG( g_logger ) << "Sync chunk sccessfully" << Logger::endl();
                chunk_server_list[chunk_server_info::join( dist->addr, dist->port )].is_need_sync = false;
                return true;
            }

            return false;
        }

        FATAL_STD_STREAM_LOG( g_logger ) << "Error Server Reply!" << Logger::endl();
        return false;
    }
    catch ( std::exception& e )
    {
        throw e;
    }
}

void master_server::sync_chunk( chunk_server_info cur_server )
{
    try
    {
        server_lock->lock_write( file_operation::write );
        INFO_STD_STREAM_LOG( g_logger )
        << "%D"
        << "Chunk server connected!, check it is need sync chunk!" << Logger::endl();
        /* 遍历不在线的chunk */
        for ( auto item : chunk_server_meta_data[cur_server.join()] )
        {
            DEBUG_STD_STREAM_LOG( g_logger )
            << "Server have " << S( chunk_server_meta_data[cur_server.join()].size() )
            << "Chunks" << Logger::endl();
            std::vector< std::string > arr;
            file::ptr cur_file( new file( std::get< 0 >( item ), master_server::max_chunk_size ) );
            if ( !cur_file )
            {
                continue;
            }
            /* 读当前这个块的元数据的地址与这个刚连接上的chunk server，地址是否相同，不同则被副本替代，需要同步 */
            cur_file->open( file_operation::read, master_server::m_db );
            cur_file->read_chunk_meta_data( std::get< 1 >( item ), arr );
            DEBUG_STD_STREAM_LOG( g_logger ) << "chunk server info: " << cur_server.addr
                                             << ":" << S( cur_server.port ) << Logger::endl();
            DEBUG_STD_STREAM_LOG( g_logger )
            << "Meta Data: " << arr[0] << ":" << arr[1] << Logger::endl();
            cur_file->close();
            if ( arr[0] == cur_server.addr && std::stoi( arr[1] ) == cur_server.port )
            {
                /* 不需要同步 */
                DEBUG_STD_STREAM_LOG( g_logger ) << "%D"
                                                 << "No Need sync chunk" << Logger::endl();
                continue;
            }
            DEBUG_STD_STREAM_LOG( g_logger ) << "%D"
                                             << "Nedd sync chunk" << Logger::endl();
            /* 需要同步 */
            IPv4Address::ptr addr = IPv4Address::Create( arr[0].c_str(), std::stoi( arr[1] ) );
            MSocket::ptr sock     = MSocket::CreateTCP( addr );

            if ( !sock->connect( addr ) )
            {
                FATAL_STD_STREAM_LOG( g_logger )
                << "sync chunk: Ask chunk data Fail!" << Logger::endl();
                server_lock->release_write();
                return;
            }

            /* 询问块数据 */
            protocol::Protocol_Struct cur(
            110, "", cur_file->get_name(), cur_file->get_path(), 0, "", {} );
            cur.customize.push_back( S( std::get< 1 >( item ) ) );
            tcpserver::send( sock, cur );

            protocol::ptr current_procotol = tcpserver::recv( remote_sock, master_server::buffer_size );

            /* 看是否接受成功 */
            if ( !current_procotol )
            {
                FATAL_STD_STREAM_LOG( g_logger ) << "%D"
                                                 << "Receive Message Error" << Logger::endl();
                server_lock->release_write();
                return;
            }

            cur              = current_procotol->get_protocol_struct();
            std::string data = "";
            if ( cur.bit == 115 )
            {
                data = cur.data;
            }
            else
            {
                FATAL_STD_STREAM_LOG( g_logger )
                << "sync chunk: Get chunk data Fail!" << Logger::endl();
                server_lock->release_write();
                return;
            }

            /* 把块数据，发给待同步的块 */
            sock->close();
            cur.reset( 108, "", cur_file->get_name(), cur_file->get_path(), 0, data, {} );
            current_procotol->set_protocol_struct( cur );
            addr = IPv4Address::Create( cur_server.addr.c_str(), cur_server.port );
            sock = MSocket::CreateTCP( addr );

            if ( !sock->connect( addr ) )
            {
                FATAL_STD_STREAM_LOG( g_logger )
                << "sync chunk: Send chunk data Fail!" << Logger::endl();
                server_lock->release_write();
                return;
            }

            tcpserver::send( sock, cur );

            current_procotol = tcpserver::recv( remote_sock, master_server::buffer_size );

            /* 看是否接受成功 */
            if ( !current_procotol )
            {
                FATAL_STD_STREAM_LOG( g_logger ) << "%D"
                                                 << "Receive Message Error" << Logger::endl();
                server_lock->release_write();
                return;
            }

            /* 改元数据表,meta_data_tab */
            cur_file->open( file_operation::write, master_server::m_db );
            cur_file->record_chunk_meta_data( std::get< 1 >( item ),
                                              cur_server.addr,
                                              cur_server.port );
            cur_file->close();
            sock->close();
        }

        INFO_STD_STREAM_LOG( g_logger )
        << "%D"
        << "Chunk server connected!, sync chunk End!" << Logger::endl();
        server_lock->release_write();
    }
    catch ( std::exception& e )
    {
        server_lock->release_write();
        throw e;
    }
}

void master_server::respond()
{
    try
    {
        /* 获取this指针 */
        master_server* self = ( master_server* )( master_server* )arg_ss.top();

        if ( !self )
        {
            return;
        }

        MSocket::ptr remote_sock = sock_ss.top();

        if ( !remote_sock->isConnected() )
        {
            return;
        }

        if ( !remote_sock )
        {
            FATAL_STD_STREAM_LOG( g_logger )
            << "The connection has not been established." << Logger::endl();
            return;
        }

        self->m_status = Normal;

        /* 初始化协议结构体 */
        protocol::ptr current_procotol;

        /* 接受信息 */
        current_procotol = tcpserver::recv( remote_sock, self->buffer_size );

        /* 看是否接受成功 */
        if ( !current_procotol )
        {
            FATAL_STD_STREAM_LOG( g_logger ) << "%D"
                                             << "Receive Message Error" << Logger::endl();
            return;
        }

        protocol::Protocol_Struct cur = current_procotol->get_protocol_struct();

        /* 执行相应的消息处理函数 */
        self->message_funcs[cur.bit]( { self, &cur, current_procotol.get(), remote_sock.get() } );

        // sock_ss.pop();
    }
    catch ( std::exception& e )
    {
        throw e.what();
    }
}

std::string master_server::encrypt_pwd( std::string pwd )
{
    const unsigned char Base64EncodeMap[64]
    = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
        'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
        'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
        'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/' };

    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3]; // store 3 byte of bytes_to_encode
    unsigned char char_array_4[4]; // store encoded character to 4 bytes
    int in_len                  = pwd.size();
    const char* bytes_to_encode = pwd.c_str();

    while ( in_len-- )
    {
        char_array_3[i++] = *( bytes_to_encode++ ); // get three bytes (24 bits)
        if ( i == 3 )
        {
            // eg. we have 3 bytes as ( 0100 1101, 0110 0001, 0110 1110) --> (010011, 010110, 000101, 101110)
            char_array_4[0] = ( char_array_3[0] & 0xfc ) >> 2; // get first 6 bits of first byte,
            char_array_4[1]
            = ( ( char_array_3[0] & 0x03 ) << 4 ) + ( ( char_array_3[1] & 0xf0 ) >> 4 ); // get last 2 bits of first byte and first 4 bit of second byte
            char_array_4[2]
            = ( ( char_array_3[1] & 0x0f ) << 2 ) + ( ( char_array_3[2] & 0xc0 ) >> 6 ); // get last 4 bits of second byte and first 2 bits of third byte
            char_array_4[3] = char_array_3[2] & 0x3f; // get last 6 bits of third byte

            for ( i = 0; ( i < 4 ); i++ )
                ret += Base64EncodeMap[char_array_4[i]];
            i = 0;
        }
    }

    if ( i )
    {
        for ( j = i; j < 3; j++ )
            char_array_3[j] = '\0';

        char_array_4[0] = ( char_array_3[0] & 0xfc ) >> 2;
        char_array_4[1] = ( ( char_array_3[0] & 0x03 ) << 4 ) + ( ( char_array_3[1] & 0xf0 ) >> 4 );
        char_array_4[2] = ( ( char_array_3[1] & 0x0f ) << 2 ) + ( ( char_array_3[2] & 0xc0 ) >> 6 );

        for ( j = 0; ( j < i + 1 ); j++ )
            ret += Base64EncodeMap[char_array_4[j]];

        while ( ( i++ < 3 ) )
            ret += '=';
    }

    return ret;
}

bool master_server::login( std::string user_name, std::string pwd )
{
    std::string got_pwd;

    this->m_db->Get( user_name, got_pwd );

    std::string encrypted = master_server::encrypt_pwd( pwd );
    DEBUG_STD_STREAM_LOG( g_logger ) << "encrypted password: " << encrypted << "%n%d";

    if ( got_pwd == encrypted )
    {
        INFO_STD_STREAM_LOG( g_logger ) << "user: " << user_name << " "
                                        << "Login success！" << Logger::endl();

        this->is_login = true;

        return true;
    }

    ERROR_STD_STREAM_LOG( g_logger )
    << "user: " << user_name << "Login failed！ %n what: The password is incorrect.！"
    << Logger::endl();
    return false;
}

bool master_server::regist( std::string user_name, std::string pwd )
{
    std::string encrypted = master_server::encrypt_pwd( pwd );

    // DEBUG_STD_STREAM_LOG( g_logger ) << "encrypted password: " << encrypted << "%n%d";

    std::string temp = "";

    this->m_db->Get( user_name, temp );
    if ( temp.empty() )
    {
        INFO_STD_STREAM_LOG( g_logger )
        << "user: " << user_name << "register success！" << Logger::endl();
        this->m_db->Put( user_name, encrypted );
        return true;
    }

    ERROR_STD_STREAM_LOG( g_logger )
    << "user: " << user_name << "register failed！ %n what: The user is already registered."
    << Logger::endl();
    return false;
}

void master_server::check_chunk_server()
{
    /* 遍历 chunk server 列表，向 chunk server 发消息 */
    try
    {
        for ( auto item : chunk_server_list )
        {
            protocol::Protocol_Struct cur( -1, "", "", "", 0, "", {} );

            star::IPv4Address::ptr addr
            = star::IPv4Address::Create( item.second.addr.c_str(), item.second.port );
            MSocket::ptr sock = star::MSocket::CreateTCP( addr );
            ;
            // sock->bind( addr );
            bool flag = sock->connect( addr );
            if ( !flag )
            {
                /* 连接 chunk server 失败 */
                master_server::chunk_server_connect_fail( item.first );
            }
            else
            {
                DEBUG_STD_STREAM_LOG( g_logger )
                << "Chunk Server: " << item.second.addr << ":" << S( item.second.port )
                << " Online!" << Logger::endl();
                tcpserver::send( sock, cur );
                available_chunk_server[item.second.join()] = ( item.second );
                available_chunk_server_name.push_back( item.first );
                if ( item.second.is_need_sync )
                {
                    /* 连接成功后，同步chunk */
                    sync_chunk( item.second );
                    item.second.is_need_sync = false;
                }
            }

            sock->close();
        }
    }
    catch ( std::exception& e )
    {
        throw e;
    }
}

void master_server::heart_beat()
{
    while ( true )
    {
        INFO_STD_STREAM_LOG( g_logger ) << "%D"
                                        << "Begin Check the chunk server!" << Logger::endl();

        available_chunk_server.clear();
        fail_chunk_server.clear();
        available_chunk_server_name.clear();
        master_server::check_chunk_server();
        sleep( 30 );
        INFO_STD_STREAM_LOG( g_logger ) << "%D"
                                        << "Check the chunk server End!" << Logger::endl();
    }
}

void master_server::chunk_server_connect_fail( std::string chunk_server_url )
{
    WERN_STD_STREAM_LOG( g_logger )
    << "Chunk Server: " << chunk_server_list[chunk_server_url].addr << ":"
    << S( chunk_server_list[chunk_server_url].port ) << " is not available" << Logger::endl();
    fail_chunk_server[chunk_server_url] = chunk_server_list[chunk_server_url];
}

/*
    客户端请求指定文件的元数据
*/
void master_server::deal_with_101( std::vector< void* > args )
{
    file_meta_data got_file;
    master_server* self           = ( master_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    MSocket::ptr remote_sock;
    remote_sock.reset( ( MSocket* )args[3] );
    std::string file_name = cur.file_name;
    std::string file_path = cur.path;
    std::vector< std::string > result;
    bool flag = self->find_file_meta_data( result, file_name, file_path );
    cur.reset( 107, self->m_sock->getLocalAddress()->toString(), file_name, file_path, 0, "", {} );
    if ( flag )
    {
        cur.data = "File Find!";
        int i    = 0;
        while ( i < result.size() )
        {
            cur.customize.push_back( result[++i] );
            cur.customize.push_back( result[++i] );
            cur.customize.push_back( result[++i] );
        }
    }
    else
    {
        cur.data = "File Not Find!";
    }

    tcpserver::send( remote_sock, cur );

    self->m_lease_control->destory_invalid_lease();
    /* 颁发一个60秒的租约 */
    self->m_lease_control->new_lease();
}

/*
    客户端上传文件
*/
void master_server::deal_with_104( std::vector< void* > args )
{
    try
    {
        master_server* self           = ( master_server* )args[0];
        protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
        MSocket::ptr remote_sock;
        remote_sock.reset( ( MSocket* )args[3] );
        std::string result = "true";

        /* 等待租约全部过期 */
        while ( !self->m_lease_control->is_all_late() )
        {
        }
        file::ptr cur_file = nullptr;

        while ( cur.data != "file End" )
        {
            std::string file_name = cur.file_name;
            std::string file_path = cur.path;
            std::string data      = cur.data;

            /* 打开文件 */
            cur_file.reset( new file( file_name, file_path, self->max_chunk_size ) );

            /* 直至把块保存成功结束循环 */
            while ( true )
            {
                IPv4Address::ptr addr = nullptr;
                MSocket::ptr sock     = nullptr;
                int random            = 0;
                /* 直至连接成功结束循环 */
                while ( true )
                {
                    /* 随机给一个在线的 chunk server 发送数据 */
                    std::random_device seed;
                    std::ranlux48 engine( seed() );
                    std::uniform_int_distribution<> distrib( 0, available_chunk_server.size() - 1 );
                    random = distrib( engine );
                    DEBUG_STD_STREAM_LOG( g_logger )
                    << "random num: " << S( random ) << Logger::endl();
                    if ( random < 0 || random >= available_chunk_server.size()
                         || random >= chunk_server_list.size() )
                    {
                        random = 0;
                    }
                    /* 与 chunk server 通讯 */
                    addr = IPv4Address::Create(
                    available_chunk_server[available_chunk_server_name[random]].addr.c_str(),
                    available_chunk_server[available_chunk_server_name[random]].port );
                    sock = MSocket::CreateTCP( addr );
                    if ( sock->connect( addr ) )
                    {
                        break;
                    }
                }

                cur.reset( 108, "", file_name, file_path, data.size(), data, {} );
                tcpserver::send( sock, cur );
                /* 等待回复判断是否成功 */
                protocol::ptr current_protocol = tcpserver::recv( sock, self->buffer_size );
                if ( !current_protocol )
                {
                    FATAL_STD_STREAM_LOG( g_logger )
                    << "%D"
                    << "get replty from chunk server error!" << Logger::endl();
                    continue;
                }
                cur = current_protocol->get_protocol_struct();
                if ( cur.bit == 116 && cur.data == "true" )
                {
                    /* 储存成功可以结束了 */
                    INFO_STD_STREAM_LOG( g_logger ) << "Store Chunk Successfully" << Logger::endl();
                    cur_file->open( file_operation::write, self->m_db );
                    /* 追加一个块的元数据 */
                    if ( !cur_file->append_meta_data(
                         available_chunk_server[available_chunk_server_name[random]].addr,
                         available_chunk_server[available_chunk_server_name[random]].port ) )
                    {
                        FATAL_STD_STREAM_LOG( g_logger )
                        << "%D"
                        << "Append meta data error!" << Logger::endl();
                        continue;
                    }

                    file_url_list->push_back( cur_file->get_url() );
                    chunk_server_meta_data[available_chunk_server[available_chunk_server_name[random]]
                                           .join()]
                    .push_back( std::tuple< std::string, size_t >( cur_file->get_url(),
                                                                   cur_file->chunks_num() ) );
                    cur_file->close();
                    break;
                }
                else
                {
                    DEBUG_STD_STREAM_LOG( g_logger ) << "Store chnk fail! Retry" << Logger::endl();
                }
            }

            data.clear();
            cur.reset( 132, "", "", "", 0, result, {} );
            tcpserver::send( remote_sock, cur );

            /* 接受一个包 */
            protocol::ptr current_protocol = tcpserver::recv( remote_sock, self->buffer_size );
            if ( !current_protocol )
            {
                FATAL_STD_STREAM_LOG( g_logger ) << "%D"
                                                 << "get package error!" << Logger::endl();
                return;
            }
            cur = current_protocol->get_protocol_struct();
            if ( cur.bit != 104 )
            {
                FATAL_STD_STREAM_LOG( g_logger ) << "%D"
                                                 << "error server bit command!" << Logger::endl();
                return;
            }
        }

        cur.reset( 131, "", "", "", 0, "finish", {} );
        tcpserver::send( remote_sock, cur );

        if ( !cur_file )
        {
            return;
        }

        /* 制作副本 */
        INFO_STD_STREAM_LOG( g_logger ) << "Begin Making copy file chunk" << Logger::endl();
        for ( size_t i = 1; i < self->copys; i++ )
        {
            std::string file_name = "copy-" + S( i ) + "-" + cur_file->get_name();
            file::ptr copy_file( new file( file_name, cur_file->get_path(), self->max_chunk_size ) );
            for ( size_t j = 0; j < cur_file->chunks_num(); j++ )
            {
                chunk::ptr from( new chunk( cur_file->get_url(), j ) );
                std::vector< std::string > res;
                from->open( file_operation::read, self->m_db );
                from->read_meta_data( res );
                from->close();
                chunk::ptr dist( new chunk( copy_file->get_url(), j ) );
                BREAK( g_logger );
                /* 直到同步副本块成功 */
                while ( true )
                {
                    /* 给这个副本块 分配一个chunk server */
                    std::random_device seed;
                    std::ranlux48 engine( seed() );
                    std::uniform_int_distribution<> distrib( 0, available_chunk_server.size() - 1 );
                    size_t random = distrib( engine );
                    dist->addr = available_chunk_server[available_chunk_server_name[random]].addr;
                    dist->port = available_chunk_server[available_chunk_server_name[random]].port;
                    /* 把内容同步到副本块上 */
                    if ( self->sync_chunk( from, dist ) )
                    {
                        BREAK( g_logger );
                        copy_file->open( file_operation::write, self->m_db );
                        copy_file->append_meta_data( dist->addr, dist->port );
                        copy_file->close();
                        chunk_server_meta_data[available_chunk_server[available_chunk_server_name[random]]
                                               .join()]
                        .push_back( std::tuple< std::string, size_t >( copy_file->get_url(),
                                                                       copy_file->chunks_num() ) );
                        file_url_list->push_back( copy_file->get_url() );
                        INFO_STD_STREAM_LOG( g_logger )
                        << "Copy file chunk " << S( j ) << "Successfully" << Logger::endl();
                        break;
                    }

                    else
                    {
                        ERROR_STD_STREAM_LOG( g_logger )
                        << "Sync Copy file chunk Error! Retry!" << Logger::endl();
                    }
                }
            }
        }
        INFO_STD_STREAM_LOG( g_logger ) << "Making copy file chunk End!" << Logger::endl();
    }
    catch ( std::exception& e )
    {
        throw e;
    }
}

/*
    修改文件路径
*/
void master_server::deal_with_117( std::vector< void* > args )
{
    pthread_mutex_lock( &mutex );
    master_server* self           = ( master_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    MSocket::ptr remote_sock;
    remote_sock.reset( ( MSocket* )args[3] );

    /* 等待租约全部过期 */
    while ( !self->m_lease_control->is_all_late() )
    {
    }

    std::string file_name     = cur.file_name;
    std::string file_path     = cur.path;
    std::string file_new_path = cur.customize[0];
    std::string res           = "ok";
    file::ptr cur_file( new file( file_name, file_path, self->max_chunk_size ) );
    if ( !cur_file->move( file_new_path ) )
    {
        res = "false";
    }

    protocol::ptr current_procotol;
    current_procotol.reset( ( protocol* )args[2] );

    for ( int i = 0; i < cur_file->chunks_num(); i++ )
    {
        chunk::ptr cur_chunk( new chunk( cur_file->get_url(), i ) );
        cur.reset(
        122, self->m_sock->getLocalAddress()->toString(), file_name, file_path, 0, "", {} );
        cur.customize.clear();
        cur.customize.push_back( file_new_path );
        cur.customize.push_back( std::to_string( cur_chunk->get_index() ) );
        std::vector< std::string > res;
        cur_chunk->open( file_operation::read, self->m_db );
        cur_chunk->read_meta_data( res );
        cur_chunk->close();
        star::IPv4Address::ptr addr
        = star::IPv4Address::Create( res[0].c_str(), std::stoi( res[1] ) );
        star::MSocket::ptr sock = star::MSocket::CreateTCP( addr );

        if ( !sock->connect( addr ) )
        {
            return;
        }
        tcpserver::send( sock, cur );

        current_procotol = tcpserver::recv( remote_sock, self->buffer_size );

        /* 看是否接受成功 */
        if ( !current_procotol )
        {
            FATAL_STD_STREAM_LOG( g_logger ) << "%D"
                                             << "Receive Message Error" << Logger::endl();
        }

        cur = current_procotol->get_protocol_struct();

        if ( cur.data != "true" && cur.bit == 124 )
        {
            ERROR_STD_STREAM_LOG( g_logger ) << "%D"
                                             << "send chunk data error" << Logger::endl();

            return;
        }
    }

    cur.reset( 125, "", file_name, file_new_path, 0, res, {} );
    tcpserver::send( remote_sock, cur );
}

/*
    用户认证
*/
void master_server::deal_with_118( std::vector< void* > args )
{
    master_server* self           = ( master_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    MSocket::ptr remote_sock;
    remote_sock.reset( ( MSocket* )args[3] );
    std::string user_name = cur.file_name;
    std::string pwd       = cur.path;
    bool flag             = self->login( user_name, pwd );
    cur.bit               = 120;
    cur.from              = self->m_sock->getLocalAddress()->toString();
    cur.file_name         = user_name;
    if ( flag )
    {
        cur.data = "true";
    }
    else
    {
        cur.data = "false";
    }

    tcpserver::send( remote_sock, cur );
}

/*
    注册用户认证信息
*/
void master_server::deal_with_119( std::vector< void* > args )
{
    master_server* self           = ( master_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    MSocket::ptr remote_sock;
    remote_sock.reset( ( MSocket* )args[3] );
    std::string user_name = cur.file_name;
    std::string pwd       = cur.path;
    bool flag             = self->regist( user_name, pwd );
    cur.reset( 121, "", user_name, "", 0, "", {} );
    if ( flag )
    {
        cur.data = "true";
    }
    else
    {
        cur.data = "false";
    }

    tcpserver::send( remote_sock, cur );
}

/*
    客户端请求已经上传的文件元数据 包含 文件名，路径
*/
void master_server::deal_with_126( std::vector< void* > args )
{
    master_server* self           = ( master_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    MSocket::ptr remote_sock;
    remote_sock.reset( ( MSocket* )args[3] );

    self->m_lease_control->destory_invalid_lease();
    self->m_lease_control->new_lease(); /* 颁发一个新租约 */

    cur.reset(
    127, self->m_sock->getLocalAddress()->toString(), "All file Meta data", "", 0, "None", {} );

    std::string all_file_name, all_file_path;
    for ( size_t i = 0; i < self->file_url_list->size(); i++ )
    {
        file::ptr cur_file( new file( self->file_url_list->get( i ), self->max_chunk_size ) );
        cur.customize.push_back( cur_file->get_name() );
        cur.customize.push_back( cur_file->get_path() );
    }

    /* 给客户端返回消息 */
    tcpserver::send( remote_sock, cur );
}
}