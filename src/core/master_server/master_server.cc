#include "./master_server.h"
#include "core/tcpServer/tcpserver.h"
#include "modules/Scheduler/scheduler.h"
#include "modules/db/database.h"
#include "modules/log/log.h"
#include "modules/meta_data/chunk.h"
#include "modules/meta_data/file.h"
#include "modules/meta_data/meta_data.h"
#include "modules/protocol/protocol.h"
#include "modules/setting/setting.h"
#include "modules/socket/address.h"
#include "modules/socket/socket.h"

#include "json/value.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

namespace star
{

static std::map< std::string, file::ptr > meta_data_tab; /* 元数据表 */
static io_lock::ptr server_io_lock( new io_lock() );

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
            cur.addr = servers[i]["address"].asString();
            cur.port = servers[i]["port"].asInt64();
            this->chunk_server_list.push_back( cur );
        }

        this->max_chunk_size = this->m_settings->get( "Max_chunk_size" ).asInt();
        this->copys          = this->m_settings->get( "copy_num" ).asInt();

        this->print_logo();
        this->m_db->open();

        /* 打开这两个集合 */
        file_name_set.reset( new levelDBSet( this->m_db, "file_name_set" ) );
        file_path_set.reset( new levelDBSet( this->m_db, "file_path_set" ) );

        /* 初始化元数据表 */
        for ( size_t i = 0; i < file_name_set->size(); i++ )
        {
            file::ptr new_file(
            new file( file_name_set->get( i ), file_path_set->get( i ), this->max_chunk_size ) );
            meta_data_tab[file_name_set->get( i )] = new_file;
        }
    }
    catch ( std::exception e )
    {
        ERROR_STD_STREAM_LOG( this->m_logger ) << "what: " << e.what() << "%n%0";
        throw e;
    };
}

bool master_server::find_file_meta_data( std::vector< std::string >& res, std::string f_name, std::string f_path )
{
    for ( auto item : meta_data_tab )
    {
        if ( item.first == f_name && item.second->get_path() == f_path )
        {
            for ( size_t i = 0; i < item.second->chunks_num(); i++ )
            {
                item.second->open( this->m_db );
                item.second->read_chunk_meta_data( i, res );
                item.second->close();
            }

            return true;
        }
    }

    ERROR_STD_STREAM_LOG( this->m_logger )
    << "file: " << f_name << "find file failed！ %n what: File does not exist."
    << "%n%0";
    return false;
}

void master_server::respond()
{
    try
    {
        /* 获取this指针 */
        master_server* self = ( master_server* )( master_server* )arg_ss.top();

        MSocket::ptr remote_sock = sock_ss.top();

        if ( !remote_sock )
        {
            FATAL_STD_STREAM_LOG( self->m_logger )
            << "The connection has not been established."
            << "%n%0";
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
            FATAL_STD_STREAM_LOG( self->m_logger ) << "%D"
                                                   << "Receive Message Error"
                                                   << "%n%0";
        }

        protocol::Protocol_Struct cur = current_procotol->get_protocol_struct();

        /* 执行相应的消息处理函数 */
        self->message_funcs[cur.bit]( { self, &cur, &current_procotol } );

        remote_sock->close();
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
    DEBUG_STD_STREAM_LOG( this->m_logger ) << "encrypted password: " << encrypted << "%n%d";

    if ( got_pwd == encrypted )
    {
        INFO_STD_STREAM_LOG( this->m_logger ) << "user: " << user_name << " "
                                              << "Login success！"
                                              << "%n%0";

        this->is_login = true;

        return true;
    }

    ERROR_STD_STREAM_LOG( this->m_logger )
    << "user: " << user_name << "Login failed！ %n what: The password is incorrect.！"
    << "%n%0";
    return false;
}

bool master_server::regist( std::string user_name, std::string pwd )
{
    std::string encrypted = master_server::encrypt_pwd( pwd );

    // DEBUG_STD_STREAM_LOG( this->m_logger ) << "encrypted password: " << encrypted << "%n%d";

    std::string temp = "";

    this->m_db->Get( user_name, temp );
    if ( temp.empty() )
    {
        INFO_STD_STREAM_LOG( this->m_logger ) << "user: " << user_name << "register success！"
                                              << "%n%0";
        this->m_db->Put( user_name, encrypted );
        return true;
    }

    ERROR_STD_STREAM_LOG( this->m_logger )
    << "user: " << user_name << "register failed！ %n what: The user is already registered."
    << "%n%0";
    return false;
}

void master_server::heart_beat()
{
    int i = 0;

    INFO_STD_STREAM_LOG( this->m_logger ) << "%D"
                                          << "Begin Check the chunk server!"
                                          << "%n%0";

    /* 遍历 chunk server 列表，向 chunk server 发消息 */
    try
    {
        for ( auto item : this->chunk_server_list )
        {

            star::IPv4Address::ptr addr = star::IPv4Address::Create( item.addr.c_str(), item.port );
            MSocket::ptr sock = star::MSocket::CreateTCP( addr );;
            // sock->bind( addr );

            if ( !sock->connect( addr ) )
            {
                /* 连接 chunk server 失败 */
                this->chunk_server_connect_fail( i );
            }

            sock->close();
            i++;
        }
    }
    catch ( std::exception& e )
    {
        throw e;
    }
    INFO_STD_STREAM_LOG( this->m_logger ) << "%D"
                                          << "Chack the chunk server End!"
                                          << "%n%0";
}

void master_server::chunk_server_connect_fail( int index )
{
    if ( index < 0 || index > this->chunk_server_list.size() )
    {
        DEBUG_STD_STREAM_LOG( this->m_logger ) << "Index out of the range!"
                                               << "%n%0";
        return;
    }

    WERN_STD_STREAM_LOG( this->m_logger )
    << "Chunk Server: " << this->chunk_server_list[index].addr << " is not available"
    << "%n%0";
    this->fail_chunk_server.push_back( this->chunk_server_list[index] );
}

/*
    客户端请求指定文件的元数据
*/
void master_server::deal_with_101( std::vector< void* > args )
{
    server_io_lock->lock_read( file_operation::read ); /* 上读锁 */
    file_meta_data got_file;
    master_server* self           = ( master_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    std::string file_name         = cur.file_name;
    std::string file_path         = cur.path;
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
    server_io_lock->release_read(); /* 解读锁 */

    tcpserver::send( remote_sock, cur );
}

/*
    客户端上传文件
*/
void master_server::deal_with_104( std::vector< void* > args )
{
    protocol::ptr current_procotol;
    master_server* self = ( master_server* )args[0];
    current_procotol.reset( ( protocol* )args[2] );
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];

    int32_t package_num = std::stoi( cur.customize[0] );
    size_t j            = 0;
    size_t k            = 0;
    std::string temp_str;
    int32_t gap = int( meta_data_tab[cur.file_name]->chunks_num() / self->chunk_server_list.size() );

    /* 接受数据包 */
    for ( size_t i = 0; i < package_num; i++ )
    {
        if ( i )
        {
            /* 继续接受后面的包 */
            current_procotol = tcpserver::recv( remote_sock, self->buffer_size );

            /* 看是否接受成功 */
            if ( !current_procotol )
            {
                FATAL_STD_STREAM_LOG( self->m_logger ) << "%D"
                                                       << "Receive Message Error"
                                                       << "%n%0";

                return;
            }
        }

        if ( j < self->chunk_server_list.size() )
        {
            cur = current_procotol->get_protocol_struct();
            /* 接收到的文件数据 */
            std::string file_name = cur.file_name;
            std::string file_path = cur.path;
            std::string data      = cur.data;
            size_t index          = std::stoi( cur.customize[0] );

            /* chunk server的空间不够 */
            if ( self->chunk_server_list[j].space < data.size() )
            {
                continue;
            }

            /* 拷贝副本 */
            for ( size_t ind = 0; ind < self->copys; ind++ )
            {
                /* 打开一个文件 */
                file::ptr cur_recv_file( new file( file_name, file_path, self->max_chunk_size ) );

                if ( ind )
                {
                    /* 副本发往不同的 chunk 服务器 */
                    cur.file_name = "copy-" + S( ind ) + "-" + file_name;
                    j++;
                    if ( j > self->chunk_server_list.size() )
                    {
                        j = 0;
                    }
                    cur_recv_file.reset( new file( file_name, file_path, self->max_chunk_size ) );
                }
                cur_recv_file->open( self->m_db );
                /* 追加一个块 */
                cur_recv_file->record_chunk_meta_data( cur_recv_file->chunks_num(), /* 记录下元数据 */
                                                       self->chunk_server_list[j].addr,
                                                       self->chunk_server_list[j].port );
                cur_recv_file->close();

                meta_data_tab[file_name] = cur_recv_file; /* 加入表中 */
                std::string all_file_name, all_file_path;
                self->file_name_set->push_back( file_name ); /* 加入新的文件名 */
                self->file_path_set->push_back( file_path ); /* 加入新的路径名 */

                cur.reset( 108, "", file_name, file_path, 0, data, {} );
                cur.customize.push_back( S( index ) );

                IPAddress::ptr addr
                = star::IPv4Address::Create( self->chunk_server_list[j].addr.c_str(),
                                             self->chunk_server_list[j].port );
                MSocket::ptr sock = star::MSocket::CreateTCP( addr );

                if ( !sock->connect( addr ) )
                {
                    return;
                }

                tcpserver::send( sock, cur );

                current_procotol = tcpserver::recv( sock, self->buffer_size );

                if ( cur.data != "true" && cur.bit == 116 )
                {
                    ERROR_STD_STREAM_LOG( self->m_logger ) << "%D"
                                                           << "Chunk Store error"
                                                           << "%n%0";
                    temp_str = "Chunk Store error";

                    return;
                }

                DEBUG_STD_STREAM_LOG( self->m_logger ) << "%D"
                                                       << "Chunk Store successfully"
                                                       << "%n%0";

                temp_str = "Chunk Store successfully";

                if ( k < gap )
                {
                    k++;
                }
                else
                {
                    k = 0;
                }
                j++;

                sock->close();
            }
        }
        else
        {
            j = 0;
        }
    }

    cur.reset( 131, self->m_sock->getLocalAddress()->toString(), "", "", 0, temp_str, {} );
    tcpserver::send( remote_sock, cur );
}

/*
    修改文件路径
*/
void master_server::deal_with_117( std::vector< void* > args )
{
    server_io_lock->lock_write( file_operation::write ); /* 上写锁 */
    pthread_mutex_lock( &mutex );
    master_server* self           = ( master_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    std::string file_name         = cur.file_name;
    std::string file_path         = cur.path;
    std::string file_new_path     = cur.customize[0];
    std::string res               = "ok";
    file::ptr cur_file            = meta_data_tab[file_name];
    if ( !cur_file->move( file_new_path ) )
    {
        res = "false";
    }

    protocol::ptr current_procotol;
    current_procotol.reset( ( protocol* )args[2] );

    for ( auto item : cur_file->get_chunk_list() )
    {
        cur.reset(
        122, self->m_sock->getLocalAddress()->toString(), file_name, file_path, 0, "", {} );
        cur.customize.clear();
        cur.customize.push_back( file_new_path );
        cur.customize.push_back( std::to_string( item->get_index() ) );
        star::IPv4Address::ptr addr = star::IPv4Address::Create( item->addr.c_str(), item->port );
        star::MSocket::ptr sock = star::MSocket::CreateTCP( addr );

        if ( !sock->connect( addr ) )
        {
            return;
        }
        tcpserver::send( sock, cur );

        sock->close();

        current_procotol = tcpserver::recv( remote_sock, self->buffer_size );

        /* 看是否接受成功 */
        if ( !current_procotol )
        {
            FATAL_STD_STREAM_LOG( self->m_logger ) << "%D"
                                                   << "Receive Message Error"
                                                   << "%n%0";
        }

        cur = current_procotol->get_protocol_struct();

        if ( cur.data != "true" && cur.bit == 124 )
        {
            ERROR_STD_STREAM_LOG( self->m_logger ) << "%D"
                                                   << "send chunk data error"
                                                   << "%n%0";

            return;
        }
    }

    cur.reset( 125, "", file_name, file_new_path, 0, res, {} );
    server_io_lock->release_write(); /* 解写锁 */
    tcpserver::send( remote_sock, cur );
}

void master_server::deal_with_118( std::vector< void* > args )
{
    master_server* self           = ( master_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    std::string user_name         = cur.file_name;
    std::string pwd               = cur.path;
    bool flag                     = self->login( user_name, pwd );
    cur.bit                       = 120;
    cur.from                      = self->m_sock->getLocalAddress()->toString();
    cur.file_name                 = user_name;
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

void master_server::deal_with_119( std::vector< void* > args )
{
    master_server* self           = ( master_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    std::string user_name         = cur.file_name;
    std::string pwd               = cur.path;
    bool flag                     = self->regist( user_name, pwd );
    cur.bit                       = 121;
    cur.file_name                 = user_name;
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

void master_server::deal_with_126( std::vector< void* > args )
{
    server_io_lock->lock_read( file_operation::read );
    master_server* self           = ( master_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    cur.clear();
    cur.bit       = 127;
    cur.file_name = "All file Meta data";
    cur.from      = self->m_sock->getLocalAddress()->toString();
    cur.data      = "None";
    std::string all_file_name, all_file_path;
    for ( size_t i = 0; i < self->file_name_set->size(); i++ )
    {
        cur.customize.push_back( self->file_name_set->get( i ) );
        cur.customize.push_back( self->file_path_set->get( i ) );
    }

    cur.customize.push_back( all_file_name );
    cur.customize.push_back( all_file_path );

    server_io_lock->release_read();
    /* 给客户端返回消息 */
    tcpserver::send( remote_sock, cur );
}

}