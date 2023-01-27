#include "./master_server.h"
#include "core/tcpServer/tcpserver.h"
#include "modules/Scheduler/scheduler.h"
#include "modules/io_lock/io_lock.h"
#include "modules/log/log.h"
#include "modules/meta_data/meta_data.h"
#include "modules/protocol/protocol.h"
#include "modules/socket/address.h"
#include "modules/socket/socket.h"

#include "json/value.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <unistd.h>

namespace star
{

static std::map< std::string, file_meta_data > meta_data_tab; /* 元数据表 */
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
    }
    catch ( std::exception e )
    {
        ERROR_STD_STREAM_LOG( this->m_logger ) << "what: " << e.what() << "%n%0";
        throw e;
    };
}

bool master_server::find_file_meta_data( file_meta_data& data, std::string f_name, std::string f_path )
{
    for ( auto item : meta_data_tab )
    {
        if ( item.first == f_name && item.second.f_path == f_path )
        {
            data = item.second;
            return true;
        }
    }

    ERROR_STD_STREAM_LOG( this->m_logger )
    << "file: " << f_name << "find file failed！ %n what: File does not exist."
    << "%n%0";
    return false;
}

bool master_server::find_chunk_meta_data( std::string f_name, int index, chunk_meta_data& data )
{
    for ( auto item : meta_data_tab )
    {
        if ( item.first == f_name )
        {
            data = item.second.chunk_list[index];
            return true;
        }
    }

    ERROR_STD_STREAM_LOG( this->m_logger )
    << "file: " << f_name << "index: " << std::to_string( index )
    << "find chunk failed！ %n what: File does not exist."
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

void master_server::ask_chunk_meta_data()
{
    protocol::Protocol_Struct t;
    t.bit       = 106;
    t.from      = this->m_sock->getLocalAddress()->toString();
    t.file_name = "None";
    t.path      = "None";

    /* 遍历 chunk server 列表，向 chunk server 发消息 */
    for ( auto item : this->chunk_server_list )
    {
        star::IPv4Address::ptr addr = star::IPv4Address::Create( item.addr.c_str(), item.port );

        // sock->bind( addr );

        if ( !this->m_sock->connect( addr ) )
        {
            return;
        }
        tcpserver::send( this->m_sock, t );

        // DEBUG_STD_STREAM_LOG( this->m_logger ) << this->m_sock->getLocalAddress()->toString() << "%n%0";

        protocol::ptr current_procotol = tcpserver::recv( this->m_sock, this->buffer_size );

        /* 看是否接受成功 */
        if ( !current_procotol )
        {
            FATAL_STD_STREAM_LOG( this->m_logger ) << "%D"
                                                   << "Receive Message Error"
                                                   << "%n%0";
        }

        protocol::Protocol_Struct cur = current_procotol->get_protocol_struct();
        chunk_meta_data got_chunk;

        /* 接受块的元数据 */
        if ( cur.bit == 102 )
        {
            int j = 0;
            int i = 0;

            while ( i < cur.customize.size() )
            {
                meta_data_tab[cur.customize[j]].chunk_list.clear();

                meta_data_tab[cur.customize[j]].f_name = cur.customize[i];
                i                                      = i + 1;

                meta_data_tab[cur.customize[j]].f_path = cur.customize[i];
                i                                      = i + 1;

                meta_data_tab[cur.customize[j]].f_size += std::stoi( cur.customize[i] );
                i = i + 1;

                meta_data_tab[cur.customize[j]].num_chunk++;

                size_t index = cur.from.find( ':' );
                cur.from = cur.from.substr( 0, index ); /* 把 ： 后的端口号去掉 */
                got_chunk.from = cur.from;

                got_chunk.port  = std::stoi( cur.path );
                got_chunk.index = std::stoi( cur.customize[i++] );

                meta_data_tab[cur.customize[j]].chunk_list.push_back( got_chunk );
                j++;
            }
        }

        this->m_sock->close();
    }
}

void master_server::deal_with_101( std::vector< void* > args )
{
    server_io_lock->lock_read( file_operation::read ); /* 上读锁 */
    file_meta_data got_file;
    master_server* self           = ( master_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    std::string file_name         = cur.file_name;
    std::string file_path         = cur.path;
    bool flag = self->find_file_meta_data( got_file, file_name, file_path );
    cur.clear();
    cur.bit       = 107;
    cur.from      = self->m_sock->getLocalAddress()->toString();
    cur.file_name = file_name;
    cur.path      = file_path;
    if ( flag )
    {
        cur.data = "File Find!";
        for ( auto item : meta_data_tab[file_name].chunk_list )
        {
            cur.customize.push_back( item.from );
            cur.customize.push_back( std::to_string( item.port ) );
            cur.customize.push_back( std::to_string( item.index ) );
        }
    }
    else
    {
        cur.data = "File Not Find!";
    }
    server_io_lock->release_read(); /* 解读锁 */

    tcpserver::send( remote_sock, cur );
}

void master_server::deal_with_102( std::vector< void* > args )
{
    server_io_lock->lock_write( file_operation::write ); /* 上写锁 */
    chunk_meta_data got_chunk;
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    size_t j                      = 0;
    size_t i                      = 0;
    size_t index                  = 0;

    while ( i < cur.customize.size() )
    {
        meta_data_tab[cur.customize[j]].chunk_list.clear();

        meta_data_tab[cur.customize[j]].f_name = cur.customize[i];
        i                                      = i + 1;

        meta_data_tab[cur.customize[j]].f_path = cur.customize[i];
        i                                      = i + 1;

        meta_data_tab[cur.customize[j]].f_size += std::stoi( cur.customize[i] );
        i = i + 1;

        meta_data_tab[cur.customize[j]].num_chunk++;

        index          = cur.from.find( ':' );
        cur.from       = cur.from.substr( 0, index ); /* 把 ： 后的端口号去掉 */
        got_chunk.from = cur.from;

        got_chunk.port  = std::stoi( cur.path );
        got_chunk.index = std::stoi( cur.customize[i++] );

        meta_data_tab[cur.customize[j]].chunk_list.push_back( got_chunk );
        j++;
    }

    server_io_lock->release_write(); /* 解写锁 */
}

void master_server::deal_with_104( std::vector< void* > args )
{
    server_io_lock->lock_write( file_operation::write ); /* 上写锁 */
    master_server* self = ( master_server* )args[0];
    protocol::ptr current_procotol;
    current_procotol.reset( ( protocol* )args[2] );
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    chunk_meta_data got_chunk;
    // pthread_mutex_lock( &mutex );
    std::string file_name = cur.file_name;
    std::string file_path = cur.path;
    got_chunk.f_name      = cur.file_name;
    got_chunk.f_path      = cur.path;
    got_chunk.data        = cur.data;
    int32_t package_num   = std::stoi( cur.customize[0] );
    got_chunk.index       = 0;
    size_t j              = 0;
    size_t k              = 0;
    std::string temp_str;

    int32_t gap
    = int( meta_data_tab[cur.file_name].chunk_list.size() / self->chunk_server_list.size() );

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

            cur.clear();
            cur = current_procotol->get_protocol_struct();

            got_chunk.f_name = cur.file_name;
            got_chunk.f_path = cur.path;
            got_chunk.data   = cur.data;
            got_chunk.index  = std::stoi( cur.customize[0] );

            for ( size_t ind = 0; ind < self->copys; ind++ )
            {
                cur.clear();
                cur.bit       = 108;
                cur.file_name = got_chunk.f_name;

                if ( ind )
                {
                    /* 副本发往不同的 chunk 服务器 */
                    cur.file_name = "copy-" + S( ind ) + "-" + got_chunk.f_name;
                    j++;
                    if ( j > self->chunk_server_list.size() )
                    {
                        j = 0;
                    }
                }

                cur.path = got_chunk.f_path;
                cur.data = got_chunk.data;
                cur.customize.push_back( S( got_chunk.index ) );

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

    // pthread_mutex_unlock( &mutex );

    try
    {
        cur.clear();
        cur.bit       = 131;
        cur.from      = self->m_sock->getLocalAddress()->toString();
        cur.file_name = file_name;
        cur.path      = file_path;
        cur.data      = temp_str;

        tcpserver::send( remote_sock, cur );
        server_io_lock->release_write(); /* 解写锁 */
        // sleep( 5 );
    }
    catch ( std::exception& e )
    {
        throw e.what();
    }
}

void master_server::deal_with_117( std::vector< void* > args )
{
    server_io_lock->lock_write( file_operation::write ); /* 上写锁 */
    pthread_mutex_lock( &mutex );
    master_server* self           = ( master_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    std::string file_name         = cur.file_name;
    std::string file_path         = cur.path;
    std::string file_new_path     = cur.customize[0];
    chunk_meta_data got_chunk;
    file_meta_data got_file;
    meta_data_tab[file_name].f_path = file_new_path;
    got_file                        = meta_data_tab[file_name];
    protocol::ptr current_procotol;
    current_procotol.reset( ( protocol* )args[2] );

    for ( auto item : got_file.chunk_list )
    {
        cur.bit       = 122;
        cur.from      = self->m_sock->getLocalAddress()->toString();
        cur.file_name = file_name;
        cur.path      = file_path;
        cur.customize.clear();
        cur.customize.push_back( file_new_path );
        cur.customize.push_back( std::to_string( item.index ) );
        star::IPv4Address::ptr addr = star::IPv4Address::Create( item.from.c_str(), item.port );
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

    pthread_mutex_unlock( &mutex );

    cur.clear();
    cur.bit       = 125;
    cur.file_name = file_name;
    cur.path      = file_new_path;
    cur.data      = "true";

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
    for ( auto item : meta_data_tab )
    {
        cur.customize.push_back( item.second.f_name );
        cur.customize.push_back( item.second.f_path );
    }

    server_io_lock->release_read();
    /* 给客户端返回消息 */
    tcpserver::send( remote_sock, cur );
}

}