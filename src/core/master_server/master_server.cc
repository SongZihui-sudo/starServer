#include "./master_server.h"
#include "core/tcpServer/tcpserver.h"
#include "modules/Scheduler/scheduler.h"
#include "modules/common/common.h"
#include "modules/db/database.h"
#include "modules/log/log.h"
#include "modules/meta_data/chunk.h"
#include "modules/meta_data/file.h"
#include "modules/meta_data/meta_data.h"
#include "modules/protocol/protocol.h"
#include "modules/setting/setting.h"
#include "modules/socket/address.h"
#include "modules/socket/socket.h"

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

std::unordered_map< std::string, std::unordered_map< std::string, std::vector< size_t > > > chunk_server_meta_data; /* 元数据表，通过 chunk server 索引 */
std::unordered_map< std::string, master_server::chunk_server_info > chunk_server_list = {}; /* 所有的 chunk server 信息 */
std::unordered_map< std::string, master_server::chunk_server_info > fail_chunk_server = {}; /* 连接不上的chunk server */
std::unordered_map< std::string, master_server::chunk_server_info > available_chunk_server; /* 在线的chunk server */
std::vector< std::string > available_chunk_server_name = {}; /* 用来使用下标随机访问服务器的名 */
std::unordered_map< std::string, std::tuple< std::string, int64_t > > crash_record; /* 记录连接端口前，发chunk的chunkserver地址，使用用户名（客户端id）索引 */

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
                if ( !arr.empty() )
                {
                    chunk_server_meta_data[chunk_server_info::join( arr[0], std::stoi( arr[1] ) )]
                                          [new_file->get_url()]
                                          .push_back( k );
                }
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

bool master_server::find_file_meta_data( std::vector< std::string >& res, std::string file_url )
{
    /* 先使用副本替换失联的 chunk server 上的chunk */
    std::vector< std::string > arr;
    file::ptr cur_file( new file( file_url, this->max_chunk_size ) );

    for ( size_t i = 0; i < cur_file->chunks_num(); i++ )
    {
        cur_file->open( file_operation::read, this->m_db );
        cur_file->read_chunk_meta_data( i, arr );
        for ( auto item : arr )
        {
            res.push_back( item );
        }
        cur_file->close();
    }

    return true;
}

void master_server::replace_unconnect_chunk( chunk_server_info unconnect_server )
{
    server_lock->lock_write( file_operation::write );
    INFO_STD_STREAM_LOG( g_logger )
    << "%D"
    << "Chunk server connect error, replace chunk begin!" << Logger::endl();
    /* 查找替换丢失的文件块元数据 */
    for ( auto iter : chunk_server_meta_data[unconnect_server.join()] )
    {
        for ( size_t i = 0; i < iter.second.size(); i++ )
        {
            chunk::ptr copy_chunk = nullptr;
            std::vector< std::string > res;
            /* 查看其副本是否在线 */
            for ( size_t j = 1; j < master_server::copys; j++ )
            {
                file::ptr origin_file( new file( std::get< 0 >( iter ), master_server::max_chunk_size ) );
                std::string copys_name = file::join_copy_name( origin_file->get_name(), j );
                copy_chunk.reset( new chunk( copys_name, origin_file->get_path(), iter.second[i] ) );
                copy_chunk->open( file_operation::read, master_server::m_db );
                copy_chunk->read_meta_data( res );
                copy_chunk->close();
                /* 块不存在 */
                if ( res.empty() )
                {
                    continue;
                }
                /* 判断副本所在的chunk server是否在线 */
                if ( fail_chunk_server[chunk_server_info::join( res[0], std::stoi( res[1] ) )]
                     .addr.empty() )
                {
                    break;
                }
            }
            if ( !copy_chunk || res.empty() )
            {
                /* 副本全部不在线 */
                ERROR_STD_STREAM_LOG( g_logger )
                << "Chunk copies are all offline and cannot be synchronized." << Logger::endl();
                server_lock->release_write();
                return;
            }
            /* 把 item 所在的 chunk server 标记为需要同步 */
            chunk_server_list[unconnect_server.join()].is_need_sync = true;
            /* 用 copy file 的元数据来替换 iter 的元数据 */
            chunk::ptr need_replace( new chunk( std::get< 0 >( iter ), iter.second[i] ) );
            need_replace->open( file_operation::write, master_server::m_db );
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
        110, "", from->get_name(), from->get_path(), 0, "", { S( from->get_index() ), S( 0 ) } );
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

        /* chunk 的 数据包，一个chunk由若干个数据包进行传输 */
        while ( true )
        {
            protocol::ptr current_procotol = tcpserver::recv( sock, master_server::buffer_size );
            /* 看是否接受成功 */
            if ( !current_procotol )
            {
                FATAL_STD_STREAM_LOG( g_logger ) << "%D"
                                                 << "Receive Message Error" << Logger::endl();
                return false;
            }
            prs = current_procotol->get_protocol_struct();
            /* 判断回复是否正确 */
            if ( prs.bit == 115 )
            {
                /* 接受数据 */
                std::string data = prs.data;
                if ( data == "chunk end" )
                {
                    break;
                }
                std::string index = prs.customize[0];
                if ( data.size() != prs.package_size )
                {
                    /* 数据不全 */
                    ERROR_STD_STREAM_LOG( g_logger )
                    << "Maybe the package is not Complete!" << Logger::endl();
                    return false;
                }

                /* 把数据发给, dist 块所在的chunk server */
                DEBUG_STD_STREAM_LOG( g_logger ) << "data: " << data << Logger::endl();
                prs.reset( 108, "", dist->get_name(), dist->get_path(), data.size(), data, { index } );
                IPAddress::ptr dist_addr = IPv4Address::Create( dist->addr.c_str(), dist->port );
                MSocket::ptr dist_sock = MSocket::CreateTCP( dist_addr );
                if ( !dist_sock->connect( dist_addr ) )
                {
                    FATAL_STD_STREAM_LOG( g_logger )
                    << "sync chunk: Ask chunk data Fail!" << Logger::endl();
                    return false;
                }
                tcpserver::send( dist_sock, prs );
                protocol::ptr current_procotol
                = tcpserver::recv( dist_sock, master_server::buffer_size );

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
                    chunk_server_list[chunk_server_info::join( dist->addr, dist->port )].is_need_sync
                    = false;
                }
                /* 回复受到数据 */
                prs.reset( 141, "", "", "", 0, "recv data ok", {} );
                tcpserver::send( sock, prs );
            }
            else
            {
                FATAL_STD_STREAM_LOG( g_logger ) << "Error Server Reply!" << Logger::endl();
                return false;
            }
        }

        return true;
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
        /* 遍历不在线的file */
        int i = 0;
        for ( auto item : chunk_server_meta_data[cur_server.join()] )
        {
            /* 遍历文件中不在线的chunk */
            for ( size_t i = 0; i < item.second.size(); i++ )
            {
                DEBUG_STD_STREAM_LOG( g_logger )
                << "Server have " << S( chunk_server_meta_data[cur_server.join()].size() )
                << "Chunks" << Logger::endl();
                std::vector< std::string > arr;
                chunk::ptr cur_chunk( new chunk( std::get< 0 >( item ), item.second[i] ) );
                if ( !cur_chunk )
                {
                    continue;
                }
                /* 读当前这个块的元数据的地址与这个刚连接上的chunk server，地址是否相同，不同则被副本替代，需要同步 */
                cur_chunk->open( file_operation::read, master_server::m_db );
                cur_chunk->read_meta_data( arr );
                DEBUG_STD_STREAM_LOG( g_logger ) << "chunk server info: " << cur_server.addr << ":"
                                                 << S( cur_server.port ) << Logger::endl();
                DEBUG_STD_STREAM_LOG( g_logger )
                << "Meta Data: " << arr[0] << ":" << arr[1] << Logger::endl();
                cur_chunk->close();
                if ( arr[0] == cur_server.addr && std::stoi( arr[1] ) == cur_server.port )
                {
                    /* 不需要同步 */
                    DEBUG_STD_STREAM_LOG( g_logger ) << "%D"
                                                     << "No Need sync chunk" << Logger::endl();
                    continue;
                }
                DEBUG_STD_STREAM_LOG( g_logger ) << "%D"
                                                 << "Need sync chunk" << Logger::endl();
                /*
                    cur_chunk 是被副本替换了的chunk
                    文件名与替换前相同，但是地址与替换前不同
                */
                std::string server_url = chunk_server_info::join( cur_chunk->addr, cur_chunk->port );
                chunk::ptr copy_chunk( new chunk( item.first, item.second[i] ) );
                if ( !master_server::sync_chunk( copy_chunk, cur_chunk ) )
                {
                    FATAL_STD_STREAM_LOG( g_logger ) << "Sync Chunk Error!" << Logger::endl();
                    return;
                }
                INFO_STD_STREAM_LOG( g_logger ) << "Sync Chunk Successfully!" << Logger::endl();
            }
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

        MSocket::ptr remote_sock = sock_ss.front();

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
    replace_unconnect_chunk( chunk_server_list[chunk_server_url] );
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
    BREAK( g_logger );
    std::string file_name = cur.file_name;
    std::string file_path = cur.path;
    std::vector< std::string > result;
    BREAK( g_logger );
    bool flag = self->find_file_meta_data( result, file::join( file_name, file_path ) );
    cur.reset( 107, self->m_sock->getLocalAddress()->toString(), file_name, file_path, 0, "", {} );
    BREAK( g_logger );
    if ( flag )
    {
        cur.data = "File Find!";
        int i    = 0;
        while ( i < result.size() )
        {
            cur.customize.push_back( result[i] );
            i++;
            cur.customize.push_back( result[i] );
            i++;
            cur.customize.push_back( result[i] );
            i++;
        }
    }
    else
    {
        cur.data = "File Not Find!";
    }
    BREAK( g_logger );
    tcpserver::send( remote_sock, cur );
    BREAK( g_logger );
    self->m_lease_control->destory_invalid_lease();
    BREAK( g_logger );
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
        int64_t cur_time   = getTime();
        chunk_server_info cur_server;
        std::string cliend_id = std::get< 0 >( crash_record[cur.customize[1]] );
        DEBUG_STD_STREAM_LOG( g_logger ) << "client id: " << cliend_id << Logger::endl();
        if ( !cliend_id.empty() || cur_time - std::get< 1 >( crash_record[cur.customize[1]] ) < 60 )
        {
            cur_server = chunk_server_list[cliend_id];
        }
        else
        {
            cliend_id  = cur.customize[1];
            cur_server = random_choice_server();
            crash_record[cliend_id]
            = std::tuple< std::string, size_t >( cur_server.join(), getTime() );
        }
        size_t sum_size     = 0;
        size_t chunks_index = 0;
        while ( true )
        {
            if ( sum_size >= self->max_chunk_size )
            {
#define XX()                                                                                            \
    if ( !cur_file )                                                                                    \
    {                                                                                                   \
        FATAL_STD_STREAM_LOG( g_logger ) << "File is not exist!" << Logger::endl();                     \
        return;                                                                                         \
    }                                                                                                   \
    BREAK( g_logger );                                                                                  \
    sum_size = 0;                                                                                       \
    chunks_index++;                                                                                     \
    cur_file->open( file_operation::write, self->m_db );                                                \
    if ( !cur_file->append_meta_data( cur_server.addr, cur_server.port ) )                              \
    {                                                                                                   \
        FATAL_STD_STREAM_LOG( g_logger ) << "%D"                                                        \
                                         << "Append meta data error!" << Logger::endl();                \
        continue;                                                                                       \
    }                                                                                                   \
    file_url_list->push_back( cur_file->get_url() );                                                    \
    chunk_server_meta_data[cur_server.join()][cur_file->get_url()].push_back( cur_file->chunks_num() ); \
    cur_file->close();

                XX();

                cur_server = random_choice_server();
                crash_record[cliend_id]
                = std::tuple< std::string, size_t >( cur_server.join(), getTime() );
            }
            else if ( cur.data == "file End" )
            {
                BREAK( g_logger );
                XX();
#undef XX
                break;
            }
            else
            {
                std::string file_name = cur.file_name;
                std::string file_path = cur.path;
                std::string data      = cur.data;
                sum_size += data.size();
                BREAK( g_logger );
                DEBUG_STD_STREAM_LOG( g_logger ) << "sum size: " << S( sum_size ) << Logger::endl();

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
                        /* 与 chunk server 通讯 */
                        addr = IPv4Address::Create( cur_server.addr.c_str(), cur_server.port );
                        sock = MSocket::CreateTCP( addr );
                        if ( sock->connect( addr ) )
                        {
                            break;
                        }
                    }

                    cur.reset( 108,
                               self->m_sock->getLocalAddress()->toString(),
                               file_name,
                               file_path,
                               data.size(),
                               data,
                               { S( chunks_index ) } );
                    tcpserver::send( sock, cur );
                    /* 等待回复判断是否成功 */
                    protocol::ptr current_protocol = tcpserver::recv( sock, self->buffer_size );
                    if ( !current_protocol )
                    {
                        FATAL_STD_STREAM_LOG( g_logger )
                        << "%D"
                        << "get reply from chunk server error!" << Logger::endl();
                        continue;
                    }
                    cur = current_protocol->get_protocol_struct();
                    if ( cur.bit == 116 && cur.data == "true" )
                    {
                        /* 储存成功可以结束了 */
                        INFO_STD_STREAM_LOG( g_logger )
                        << "Package Chunk Successfully" << Logger::endl();

                        break;
                    }
                    else
                    {
                        DEBUG_STD_STREAM_LOG( g_logger )
                        << "Store Package fail! Retry" << Logger::endl();
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
                    FATAL_STD_STREAM_LOG( g_logger )
                    << "%D"
                    << "error server bit command!" << Logger::endl();
                    return;
                }
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
            std::string copy_file_name = "copy-" + S( i ) + "-" + cur_file->get_name();
            file::ptr copy_file( new file( copy_file_name, cur_file->get_path(), self->max_chunk_size ) );
            for ( size_t j = 0; j < cur_file->chunks_num(); j++ )
            {
                chunk::ptr from( new chunk( cur_file->get_url(), j ) );
                std::vector< std::string > res;
                from->open( file_operation::read, self->m_db );
                from->read_meta_data( res );
                from->close();
                chunk::ptr dist( new chunk( copy_file->get_url(), j ) );
                BREAK( g_logger );
                /* 给这个副本块 分配一个不同chunk server */
                chunk_server_info random_server = random_choice_server();
                while ( random_server.join() == chunk_server_info::join( from->addr, from->port ) )
                {
                    random_server = random_choice_server();
                }
                dist->addr = random_server.addr;
                dist->port = random_server.port;
                /* 直到同步副本块成功 */
                size_t k = 0;
                while ( true )
                {
                    /* 把内容同步到副本块上 */
                    if ( self->sync_chunk( from, dist ) )
                    {
                        BREAK( g_logger );
                        copy_file->open( file_operation::write, self->m_db );
                        copy_file->append_meta_data( dist->addr, dist->port );
                        copy_file->close();
                        chunk_server_meta_data[random_server.join()][copy_file->get_url()].push_back(
                        copy_file->chunks_num() );
                        file_url_list->push_back( copy_file->get_url() );
                        INFO_STD_STREAM_LOG( g_logger )
                        << "Copy file chunk " << S( j ) << "Successfully" << Logger::endl();
                        break;
                    }
                    else
                    {
                        if ( k > 5 )
                        {
                            break;
                        }
                        ERROR_STD_STREAM_LOG( g_logger )
                        << "Sync Copy file chunk Error! Retry!" << Logger::endl();
                    }
                    k++;
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
    master_server* self           = ( master_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    MSocket::ptr remote_sock;
    remote_sock.reset( ( MSocket* )args[3] );

    std::string file_name     = cur.file_name;
    std::string file_path     = cur.path;
    std::string file_new_path = cur.customize[0];
    std::string res           = "ok";
    std::string pre_file_url  = file::join( file_name, file_path );
    bool is_exist             = false;
    for ( size_t i = 0; i < file_url_list->size(); i++ )
    {
        if ( file_url_list->get( i ) == pre_file_url )
        {
            is_exist = true;
            break;
        }
    }

    if ( !is_exist )
    {
        return;
    }

    for ( size_t j = 0; j < self->copys; j++ )
    {
        std::string temp_file_name = file_name;
        if ( j )
        {
            temp_file_name = file::join_copy_name( file_name, j );
        }
        file::ptr cur_file( new file( temp_file_name, file_path, self->max_chunk_size ) );
        cur_file->open( file_operation::read, self->m_db );
        cur_file->close();
        file_url_list->remove( cur_file->get_url() );

        std::string server_addr;
        int16_t server_port;
        PRINT_LOG( g_logger, "%p %d%n", "DEBUG", cur_file->chunks_num() );
        for ( int i = 0; i < cur_file->chunks_num(); i++ )
        {
            chunk::ptr cur_chunk( new chunk( cur_file->get_url(), i ) );

            cur.reset( 122,
                       self->m_sock->getLocalAddress()->toString(),
                       temp_file_name,
                       file_path,
                       0,
                       "",
                       {} );
            cur.customize.clear();
            cur.customize.push_back( file_new_path );
            cur.customize.push_back( std::to_string( cur_chunk->get_index() ) );

            std::vector< std::string > res;
            cur_chunk->open( file_operation::read, self->m_db );
            cur_chunk->read_meta_data( res );
            cur_chunk->close();

            if ( res.empty() )
            {
                return;
            }

            server_addr = res[0];
            server_port = std::stoi( res[1] );

            star::IPv4Address::ptr addr
            = star::IPv4Address::Create( res[0].c_str(), std::stoi( res[1] ) );
            star::MSocket::ptr sock = star::MSocket::CreateTCP( addr );
            DEBUG_STD_STREAM_LOG( g_logger ) << "address: " << addr->toString() << Logger::endl();
            if ( !sock->connect( addr ) )
            {
                return;
            }
            tcpserver::send( sock, cur );

            protocol::ptr current_procotol = tcpserver::recv( sock, self->buffer_size );

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
            chunk_server_meta_data[chunk_server_info::join( server_addr, server_port )]
                                  [cur_file->get_url()]
                                  .push_back( i );
        }
        if ( !chunk_server_meta_data[chunk_server_info::join( server_addr, server_port )].empty() )
        {
            for ( std::unordered_map< std::string, std::vector< size_t > >::iterator iter
                  = chunk_server_meta_data[chunk_server_info::join( server_addr, server_port )]
                    .begin();
                  iter
                  != chunk_server_meta_data[chunk_server_info::join( server_addr, server_port )]
                     .end(); )
            {
                if ( iter->first == pre_file_url )
                {
                    iter = chunk_server_meta_data[chunk_server_info::join( server_addr, server_port )]
                           .erase( iter );
                }
                else
                {
                    iter++;
                }
            }
        }
        cur_file->open( file_operation::write, self->m_db );
        if ( !cur_file->move( file_new_path ) )
        {
            res = "false";
        }
        cur_file->close();
        file_url_list->push_back( cur_file->get_url() );
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

    // self->m_lease_control->destory_invalid_lease();
    self->m_lease_control->new_lease(); /* 颁发一个新租约 */

    BREAK( g_logger );
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

master_server::chunk_server_info master_server::random_choice_server()
{
    /* 随机给一个在线的 chunk server 发送数据 */
    std::random_device seed;
    std::ranlux48 engine( seed() );
    std::uniform_int_distribution<> distrib( 0, available_chunk_server.size() - 1 );
    size_t random = distrib( engine );
    DEBUG_STD_STREAM_LOG( g_logger ) << "random num: " << S( random ) << Logger::endl();
    if ( random < 0 || random >= available_chunk_server.size()
         || random >= chunk_server_list.size() )
    {
        random = 0;
    }
    return available_chunk_server[available_chunk_server_name[random]];
}

/*
    文件重命名
*/
void master_server::deal_with_134( std::vector< void* > args )
{
    master_server* self           = ( master_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    MSocket::ptr remote_sock;
    remote_sock.reset( ( MSocket* )args[3] );

    std::string file_name     = cur.file_name;
    std::string file_path     = cur.path;
    std::string file_new_name = cur.customize[0];
    std::string res           = "ok";
    std::string pre_file_url  = file::join( file_name, file_path );
    bool is_exist             = false;
    for ( size_t i = 0; i < file_url_list->size(); i++ )
    {
        if ( file_url_list->get( i ) == pre_file_url )
        {
            is_exist = true;
            break;
        }
    }

    if ( !is_exist )
    {
        return;
    }

    for ( size_t j = 0; j < self->copys; j++ )
    {
        std::string temp_file_name = file_name;
        if ( j )
        {
            temp_file_name = file::join_copy_name( file_name, j );
        }
        file::ptr cur_file( new file( temp_file_name, file_path, self->max_chunk_size ) );
        cur_file->open( file_operation::read, self->m_db );
        cur_file->close();
        file_url_list->remove( cur_file->get_url() );

        std::string server_addr;
        int16_t server_port;
        PRINT_LOG( g_logger, "%p %d%n", "DEBUG", cur_file->chunks_num() );
        for ( int i = 0; i < cur_file->chunks_num(); i++ )
        {
            chunk::ptr cur_chunk( new chunk( cur_file->get_url(), i ) );

            cur.reset( 137,
                       self->m_sock->getLocalAddress()->toString(),
                       temp_file_name,
                       file_path,
                       0,
                       "",
                       {} );
            cur.customize.clear();
            cur.customize.push_back( file_new_name );
            cur.customize.push_back( std::to_string( cur_chunk->get_index() ) );

            std::vector< std::string > res;
            cur_chunk->open( file_operation::read, self->m_db );
            cur_chunk->read_meta_data( res );
            cur_chunk->close();

            if ( res.empty() )
            {
                return;
            }

            server_addr = res[0];
            server_port = std::stoi( res[1] );

            star::IPv4Address::ptr addr
            = star::IPv4Address::Create( res[0].c_str(), std::stoi( res[1] ) );
            star::MSocket::ptr sock = star::MSocket::CreateTCP( addr );
            DEBUG_STD_STREAM_LOG( g_logger ) << "address: " << addr->toString() << Logger::endl();
            if ( !sock->connect( addr ) )
            {
                return;
            }
            tcpserver::send( sock, cur );

            protocol::ptr current_procotol = tcpserver::recv( sock, self->buffer_size );

            /* 看是否接受成功 */
            if ( !current_procotol )
            {
                FATAL_STD_STREAM_LOG( g_logger ) << "%D"
                                                 << "Receive Message Error" << Logger::endl();
            }

            cur = current_procotol->get_protocol_struct();

            if ( cur.data != "true" && cur.bit == 139 )
            {
                ERROR_STD_STREAM_LOG( g_logger ) << "%D"
                                                 << "send chunk data error" << Logger::endl();

                return;
            }
            chunk_server_meta_data[chunk_server_info::join( server_addr, server_port )]
                                  [cur_file->get_url()]
                                  .push_back( i );
        }
        if ( !chunk_server_meta_data[chunk_server_info::join( server_addr, server_port )].empty() )
        {
            for ( std::unordered_map< std::string, std::vector< size_t > >::iterator iter
                  = chunk_server_meta_data[chunk_server_info::join( server_addr, server_port )]
                    .begin();
                  iter
                  != chunk_server_meta_data[chunk_server_info::join( server_addr, server_port )]
                     .end(); )
            {
                if ( iter->first == pre_file_url )
                {
                    iter = chunk_server_meta_data[chunk_server_info::join( server_addr, server_port )]
                           .erase( iter );
                }
                else
                {
                    iter++;
                }
            }
        }
        cur_file->open( file_operation::write, self->m_db );
        std::string rename_new_name = file_new_name;
        if ( j )
        {   
            rename_new_name = file::join_copy_name(rename_new_name, j);
        }
        if ( !cur_file->rename( rename_new_name ) )
        {
            res = "false";
        }
        cur_file->close();
        file_url_list->push_back( cur_file->get_url() );
    }

    cur.reset( 140, "", file_name, file_new_name, 0, res, {} );
    tcpserver::send( remote_sock, cur );
}

/*
    删除文件的元数据
*/
void master_server::deal_with_135( std::vector< void* > args )
{
    master_server* self           = ( master_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    MSocket::ptr remote_sock;
    remote_sock.reset( ( MSocket* )args[3] );

    /* 等待租约全部过期 */
    BREAK( g_logger );
    while ( !self->m_lease_control->is_all_late() )
    {
    }
    std::string file_name = cur.file_name;
    std::string file_path = cur.path;

    std::string reply = "true";
    /* 删除副本及文件本体 */
    for ( size_t i = 0; i < self->copys; i++ )
    {
        std::string cur_file_name = file_name;
        if ( i )
        {
            cur_file_name = file::join_copy_name( file_name, i );
        }

        /* 打开文件 */
        file::ptr fp( new file( cur_file_name, file_path, self->max_chunk_size ) );
        fp->open( file_operation::read, self->m_db );
        fp->close();
        std::string server_address = "";
        int64_t server_port        = 0;
        /* 删除文件的元数据 与 数据 */
        for ( size_t j = 0; j < fp->chunks_num(); j++ )
        {
            /* 与 chunk server 通讯，删数据 */
            std::vector< std::string > res;
            fp->open( file_operation::read, self->m_db );
            fp->read_chunk_meta_data( j, res );
            fp->close();
            server_address = res[0];
            server_port    = std::stoi( res[1] );
            IPv4Address::ptr addr = IPv4Address::Create( server_address.c_str(), server_port );
            DEBUG_STD_STREAM_LOG( g_logger ) << "address: " << addr->toString() << Logger::endl();
            MSocket::ptr sock = MSocket::CreateTCP( addr );

            /* 查看是否连接成功 */
            if ( !sock->connect( addr ) )
            {
                return;
            }

            cur.reset( 113,
                       self->m_sock->getLocalAddress()->toString(),
                       fp->get_name(),
                       fp->get_path(),
                       0,
                       "",
                       { S( j ) } );
            tcpserver::send( sock, cur );
            protocol::ptr cur_protocol = tcpserver::recv( sock, self->buffer_size );
            if ( !cur_protocol )
            {
                FATAL_STD_STREAM_LOG( g_logger ) << "%D"
                                                 << "Receive Message Error" << Logger::endl();
                return;
            }

            cur = cur_protocol->get_protocol_struct();
            if ( cur.bit == 129 && cur.data == "true" )
            {
                INFO_STD_STREAM_LOG( g_logger )
                << "%D"
                << "Delete the chunk data Successfully!" << Logger::endl();
            }
            else
            {
                FATAL_STD_STREAM_LOG( g_logger ) << "Delete the chunk data Fail!" << Logger::endl();
                return;
            }

            fp->open( file_operation::write, self->m_db );
            bool flag = fp->del_chunk_meta_data( j );
            fp->close();
            if ( !flag )
            {
                return;
            }
        }
        file_url_list->remove( fp->join() );
        BREAK( g_logger );
        if ( !chunk_server_meta_data[chunk_server_info::join( server_address, server_port )].empty() )
        {
            for ( std::unordered_map< std::string, std::vector< size_t > >::iterator iter
                  = chunk_server_meta_data[chunk_server_info::join( server_address, server_port )]
                    .begin();
                  iter
                  != chunk_server_meta_data[chunk_server_info::join( server_address, server_port )]
                     .end(); )
            {
                if ( iter->first == fp->join() )
                {
                    iter = chunk_server_meta_data[chunk_server_info::join( server_address, server_port )]
                           .erase( iter );
                    BREAK( g_logger );
                }
                else
                {
                    iter++;
                }
            }
        }
        BREAK( g_logger );
    }

    cur.reset( 138, self->m_sock->getLocalAddress()->toString(), file_name, file_path, reply.size(), reply, {} );
    tcpserver::send( remote_sock, cur );
}

}