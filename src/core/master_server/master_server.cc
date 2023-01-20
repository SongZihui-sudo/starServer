#include "./master_server.h"
#include "modules/log/log.h"
#include "modules/socket/address.h"
#include "modules/socket/socket.h"
#include "json/value.h"
#include <cstring>
#include <exception>
#include <string>

namespace star
{
static Scheduler::ptr master_server_scheduler; /* 调度器 */
static Threading::ptr master_server_thread;    /* 服务器线程 */
void* c_args;
static thread_local MSocket::ptr remote_sock = nullptr;

master_server::master_server()
{
    /* 读取设置 */
    star::config::ptr Settings( new star::config( "./master_server_settings.json" ) );
    this->m_settings = Settings;

    /* 配置服务器 */
    this->m_status = INIT;
    try
    {
        std::string db_path = Settings->get( "DBpath" ).asString();
        std::string db_name = Settings->get( "DBname" ).asString();
        this->m_db.reset( new star::levelDB( db_name, db_path ) ); /* 初始化数据库 */

        this->m_name = Settings->get( "ServerName" ).asString();

        this->m_logger.reset( STAR_NAME( "MASTER_SERVER_LOGGER" ) );

        const char* addr = new char[100];
        strcpy( const_cast< char* >( addr ), Settings->get( "master_server" )["address"].asCString() );

        int port     = Settings->get( "master_server" )["port"].asInt();
        this->m_sock = MSocket::CreateTCPSocket(); /* 创建一个TCP Socket */

        if ( Settings->get( "AddressType" ).asString() == "IPv4" )
        {
            IPv4Address::ptr temp( new IPv4Address() );
            temp = IPv4Address::Create( addr, port );
            this->m_sock->bind( temp ); /* 绑定地址 */
        }
        else if ( Settings->get( "AddressType" ).asString() == "IPv6" )
        {
            IPv6Address::ptr temp( new IPv6Address() );
            temp = IPv6Address::Create( addr, port );
            this->m_sock->bind( temp ); /* 绑定地址 */
        }
        else
        {
            ERROR_STD_STREAM_LOG( this->m_logger ) << "Unknown address type！"
                                                   << "%n%0";
            return;
        }
        /* 读取chunk server 的信息 */
        Json::Value servers = this->m_settings->get( "chunk_server" );
        for ( int i = 0; i < servers.size(); i++ )
        {
            chunk_server_info cur;
            cur.addr = servers[i]["address"].asString();
            cur.port = servers[i]["port"].asInt64();
            this->chunk_server_list.push_back( cur );
        }

        /* 释放内存 */
        delete[] addr;
    }
    catch ( std::exception e )
    {
        ERROR_STD_STREAM_LOG( this->m_logger ) << "what: " << e.what() << "%n%0";
        throw e;
    };
}

void master_server::wait()
{
    this->print_logo();
    /* 运行调度器 */
    master_server_scheduler.reset( new Scheduler( 10, 10 ) );
    master_server_scheduler->run(); /* 等待进行任务调度 */
    /* 监听 socket 连接 */
    this->m_status = Normal;

    /* 设置 this 指针 */
    c_args = this;
    /* 新建一个线程进行等待*/
    int index = 0;
    int i     = 0;
    this->m_sock->listen();

    /* 阻塞线程，进行等待 */
    INFO_STD_STREAM_LOG( this->m_logger ) << std::to_string( getTime() ) << " <-----> "
                                          << "Master Server Start Listening!"
                                          << "%n%0";
    
    while ( true )
    {
        remote_sock = this->m_sock->accept();
        if ( remote_sock )
        {
            MESSAGE_MAP( SOCKET_CONNECTS, 10 );
            INFO_STD_STREAM_LOG( this->m_logger )
            << std::to_string( getTime() ) << " <-----> "
            << "New Connect Form:" << this->m_sock->getRemoteAddress()->toString() << "%n%0";
            Register( SOCKET_CONNECTS, index, 1, respond, "respond" + std::to_string( index ) ); /* 注册一个任务 */
            index++;
            Register( SOCKET_CONNECTS, index, 4 ); /* 任务调度 */

            if ( star::Schedule_args.empty() )
            {
                /* 取指令 */
                star::Schedule_args = SOCKET_CONNECTS[i];
                i++;
            }

            while ( !SOCKET_CONNECTS[i].empty() )
            {
                if ( star::Schedule_answer == 1 )
                {
                    /* 取指令 */
                    star::Schedule_args.clear();
                    star::Schedule_args   = SOCKET_CONNECTS[i];
                    star::Schedule_answer = 0;
                    i++;
                }
                else if ( star::Schedule_answer == 2 )
                {
                    FATAL_STD_STREAM_LOG( this->m_logger ) << "Server exit！"
                                                           << "%n%0";
                    star::Schedule_answer = 0;
                    return;
                }
                else
                {
                    this->m_status = COMMAND_ERROR;
                    WERN_STD_STREAM_LOG( this->m_logger ) << "Bad server flag！"
                                                          << "%n%0";
                }
            }
            index++;
        }
    }
}

void master_server::close()
{
    /* 退出服务器程序 */
    WERN_STD_STREAM_LOG( this->m_logger ) << "Server program exit！"
                                          << "%n%0";
    exit( -1 );
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
    /* 获取this指针 */
    master_server* self = ( master_server* )c_args;

    if ( !remote_sock )
    {
        FATAL_STD_STREAM_LOG( self->m_logger ) << "The connection has not been established."
                                               << "%n%0";
        return;
    }

    self->m_status = Normal;

    /* 缓冲区 */
    int length   = self->m_settings->get( "BufferSize" ).asInt();
    char* buffer = new char[length];
    /* 接受信息 */
    remote_sock->recv( buffer, length, 0 );

    /* 解析字符串 */
    star::protocol::Protocol_Struct current_procotol; /* 初始化协议结构体 */
    self->m_protocol.reset( new protocol( "Chunk-Procotol", current_procotol ) ); /* 协议解析器 */
    self->m_protocol->toJson( buffer ); /* 把缓冲区的字符串转换成json */
    self->m_protocol->Deserialize();    /* 反序列化json成结构体 */

    /* switch 中用到的变量 */

    /* 判断，请求内容 */
    switch ( current_procotol.bit )
    {

#define XX()                                                                               \
    self->m_protocol->set_protocol_struct( current_procotol );                             \
    temp2 = self->m_protocol->toStr();                                                     \
    self->m_sock->send( temp2.c_str(), temp2.size(), 0 );

        /* 标识为101，向客户端回复文件元数据 */
        case 101:
            break;
        default:
            ERROR_STD_STREAM_LOG( self->m_logger ) << "Unknown server instruction！"
                                                   << "%n%0";
            break;

#undef XX
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