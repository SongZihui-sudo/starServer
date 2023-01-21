#include "./master_server.h"
#include "core/tcpServer/tcpserver.h"
#include "modules/log/log.h"
#include "modules/protocol/protocol.h"
#include "modules/socket/address.h"
#include "modules/socket/socket.h"

#include "json/value.h"
#include <cstddef>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>

namespace star
{
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

        this->print_logo();
    }
    catch ( std::exception e )
    {
        ERROR_STD_STREAM_LOG( this->m_logger ) << "what: " << e.what() << "%n%0";
        throw e;
    };
}

bool master_server::find_file_meta_data( file_meta_data& data, std::string f_name )
{
    for ( auto item : this->meta_data_tab )
    {
        if ( item.first == f_name )
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
    for ( auto item : this->meta_data_tab )
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

void master_server::Split_file( std::string f_name,
                                const char* f_data,
                                std::string path,
                                std::map< std::string, file_meta_data >& data_tab )
{
    int i            = 0;
    int index        = 0;
    std::string temp = nullptr;
    while ( *f_data )
    {
        if ( i < this->max_chunk_size )
        {
            temp.push_back( *f_data );
        }
        else
        {
            /* 达到最大块大小，生成一个块 */
            chunk_meta_data cur( f_name, index, temp.c_str(), path, i, 0, 0 );
            data_tab[f_name].chunk_list.push_back( cur );
            index++;
            temp.clear();
            i = 0;
            data_tab[f_name].f_size += i;
        }
        i++;
    }

    if ( !temp.empty() )
    {
        chunk_meta_data cur( f_name, index, temp.c_str(), path, i, 0, 0 );
        data_tab[f_name].chunk_list.push_back( cur );
    }

    data_tab[f_name].f_name    = f_name;
    data_tab[f_name].num_chunk = data_tab[f_name].chunk_list.size();
    data_tab[f_name].f_size += i;

    this->meta_data_tab[f_name] = data_tab[f_name];
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

        /* 循环接受客户端的消息 */
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

        INFO_STD_STREAM_LOG( self->m_logger ) << current_procotol->toStr() << "%n%0";
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
    std::string got_pwd = nullptr;
    this->m_db->Get( user_name, got_pwd );
    if ( got_pwd == pwd )
    {
        INFO_STD_STREAM_LOG( this->m_logger ) << "user: " << user_name << "Login success！"
                                              << "%n%0";
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
    std::string temp      = "";

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

}

#include "../../modules/setting/setting.cc"