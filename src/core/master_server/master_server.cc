#include "./master_server.h"

namespace star
{
static Scheduler::ptr master_server_scheduler; /* 调度器 */
static Threading::ptr master_server_thread;    /* 服务器线程 */
void* c_args;

master_server::master_server()
{
    /* 读取设置 */
    star::config::ptr Settings( new star::config( "./master_server_settings.json" ) );
    this->m_settings = Settings;

    /* 配置服务器 */
    this->m_status = INIT;

    std::string db_path = Settings->get( "DBpath" ).asString();
    std::string db_name = Settings->get( "DBname" ).asString();
    this->m_db.reset( new star::levelDB( db_name, db_path ) ); /* 初始化数据库 */

    this->m_name = Settings->get( "ServerName" ).asString();

    this->m_logger.reset( STAR_NAME( "MASTER_SERVER_LOGGER" ) );

    const char* addr = Settings->get( "IPAddress" ).asCString();
    int port         = Settings->get( "Port" ).asInt();
    this->m_sock     = MSocket::CreateTCPSocket(); /* 创建一个TCP Socket */

    if ( Settings->get( "AddressType" ).asString() == "IPv4" )
    {
        IPv4Address::ptr temp( new IPv4Address() );
        this->m_sock->bind( temp ); /* 绑定地址 */
    }
    else if ( Settings->get( "AddressType" ).asString() == "IPv6" )
    {
        IPv6Address::ptr temp( new IPv6Address() );
        this->m_sock->bind( temp ); /* 绑定地址 */
    }
    else
    {
        ERROR_STD_STREAM_LOG( this->m_logger ) << "Unknown address type！"
                                               << "%n%0";
        return;
    }
}

void master_server::wait()
{
    /* 运行调度器 */
    master_server_scheduler.reset( new Scheduler( 10, 10 ) );
    master_server_scheduler->run(); /* 等待进行任务调度 */
    /* 监听 socket 连接 */
    this->m_status = Normal;
    MESSAGE_MAP( SOCKET_CONNECTS, 10 );
    /* 设置 this 指针 */
    c_args = this;
    /* 新建一个线程进行等待*/
    int index = 0;
    int i     = 0;
    /* 阻塞线程，进行等待 */
    while ( true )
    {
        if ( this->m_sock->listen() )
        {
            index++;
            insert_message( SOCKET_CONNECTS, index, 1, respond, "respond" + std::to_string( index ) ); /* 注册一个任务 */
            insert_message( SOCKET_CONNECTS, index, 4 ); /* 任务调度 */
            if ( star::Schedule_args.empty() )
            {
                /* 取指令 */
                star::Schedule_args = SOCKET_CONNECTS[i];
                i++;
            }
            else if ( star::Schedule_answer == 1 )
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
        else
        {
            /* 休眠 1s */
            sleep( 1 );
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

    self->m_status = Normal;

    /* 缓冲区 */
    int length   = self->m_settings->get( "BufferSize" ).asInt();
    char* buffer = new char[length];
    /* 接受信息 */
    self->m_sock->recv( buffer, length, 0 );

    /* 解析字符串 */
    star::protocol::Protocol_Struct current_procotol; /* 初始化协议结构体 */
    self->m_protocol.reset( new protocol( "Chunk-Procotol", current_procotol ) ); /* 协议解析器 */
    self->m_protocol->toJson( buffer ); /* 把缓冲区的字符串转换成json */
    self->m_protocol->Deserialize();    /* 反序列化json成结构体 */

    /* switch 中用到的变量 */
    chunk_meta_data c_chunk;
    const char* temp  = "";
    std::string temp2 = "";
    std::vector< chunk_meta_data > temp_arr;
    std::string* temp3 = new std::string;
    file_meta_data temp1;
    int temp4 = 0;
    std::map< std::string, file_meta_data > temp5;
    std::string temp6 = "";
    int temp8 = 0;

    /* 判断，请求内容 */
    switch ( current_procotol.bit )
    {

#define XX()                                                                               \
    self->m_protocol->set_protocol_struct( current_procotol );                             \
    temp2 = self->m_protocol->toStr();                                                     \
    self->m_sock->send( temp2.c_str(), temp2.size(), 0 );

        /* 标识为101，向客户端回复文件元数据 */
        case 101:
            temp2 = current_procotol.file_name;
            /* 查到文件的元数据 */
            self->find_file_meta_data( temp1, temp2 );
            /* 再把文件的元数据转换为协议结构体，发回去 */
            current_procotol = temp1.toProrocol();
            XX()
            break;
        /* 标识为102，向客户端回复指定块的元数据 */
        case 102:
            temp2 = current_procotol.file_name;
            temp4 = *( int* )current_procotol.customize[0];
            self->find_chunk_meta_data( temp2, temp4, c_chunk );
            /* 把块元数据转换为协议结构体 */
            current_procotol = temp1.toProrocol();
            XX()
            break;
        /* 标识为3，接受 chunk server 发过来的块元数据信息 */
        case 103:
            temp4 = current_procotol.data.size();
            for ( size_t i = 0; i < temp4; i++ )
            {
                c_chunk.f_path = current_procotol.path[i].c_str();
                c_chunk.data   = current_procotol.data[i].c_str();
                temp4          = *( int* )current_procotol.customize[i];
                c_chunk.index  = temp4;
                temp3          = ( std::string* )current_procotol.customize[i + 1];
                c_chunk.f_name = temp3->c_str();
                self->meta_data_tab[temp3->c_str()].f_name = temp3->c_str();
                self->meta_data_tab[temp3->c_str()].num_chunk++;
                temp4 = *( int* )current_procotol.customize[i + 2];
                self->meta_data_tab[temp3->c_str()].f_size += temp4;
                self->meta_data_tab[temp3->c_str()].chunk_list.push_back( c_chunk );
            }
            break;
        /* 标识为4，关闭服务器 */
        case 104:
            self->close();
            break;
        /* 标识为5，用户上传数据 */
        case 105:
            /* 根据用户上传的文件数据，划分块，然后在分发给chunk server */
            for ( size_t i = 0; i < current_procotol.data.size(); i++ )
            {
                temp2 += current_procotol.data[i];
            }

            for ( size_t i = 0; i < current_procotol.data.size(); i++ )
            {
                temp6 += current_procotol.path[i];
            }
            self->Split_file( current_procotol.file_name, temp2.c_str(), temp6, temp5 );
            /* 向 chunk server 分发块*/
            temp8 = 0;
            for ( auto item : temp5 )
            {
                /* 协议 */
                current_procotol.bit = 1;
                current_procotol.data.push_back(item.second.chunk_list[temp8].data) ;
                current_procotol.file_name = item.first;
                current_procotol.from = self->m_sock->getLocalAddress()->toString();
                current_procotol.path.push_back(item.second.chunk_list[0].f_path);
                current_procotol.file_size = item.second.f_size; 
                /* 发送 */
                XX();
                temp8++;
            }
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