#include "./chunk_server.h"

namespace star
{

static Threading::ptr chunk_server_thread;    /* 服务器线程 */
static Scheduler::ptr chunk_server_scheduler; /* 调度器 */
void* c_args;

chunk_server::chunk_server()
{
    /* 读取服务器的配置 */
    star::config::ptr Settings( new star::config( "./chunk_server_settings.json" ) );
    this->m_settings = Settings;

    /* 进行设置 */
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

    this->m_status = INIT;

    std::string db_path = Settings->get( "DBpath" ).asString();
    std::string db_name = Settings->get( "DBname" ).asString();
    this->m_db.reset( new star::levelDB( db_name, db_path ) ); /* 初始化数据库 */

    this->m_name = Settings->get( "ServerName" ).asString();

    this->m_logger.reset( STAR_NAME( "CHUNK_SERVER_LOGGER" ) );
}

void chunk_server::wait()
{
    /* 运行调度器 */
    chunk_server_scheduler.reset( new Scheduler( 10, 10 ) );
    chunk_server_scheduler->run(); /* 等待进行任务调度 */
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

void chunk_server::respond()
{
    /* 获取this指针 */
    chunk_server* self = ( chunk_server* )c_args;

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
    chunk_meta_data c_chunk;
    const char* temp  = "";
    std::string temp2 = "";
    std::vector< chunk_meta_data > temp_arr;
    std::string* temp3 = new std::string;

    /* 判断，请求内容 */
    switch ( current_procotol.bit )
    {

#define YY()                                                                               \
    self->m_protocol->set_protocol_struct( current_procotol );                             \
    self->m_protocol->Serialize();                                                         \
    temp = self->m_protocol->toStr().c_str();                                              \
    self->m_sock->send( temp, self->m_protocol->toStr().size(), 0 );

#define TT()                                                                               \
    current_procotol.bit = 1;                                                              \
    current_procotol.path.clear();                                                         \
    current_procotol.customize.clear();                                                    \
    current_procotol.data.clear();                                                         \
    current_procotol.data.push_back( "OK" );                                               \
    current_procotol.file_name.clear();                                                    \
    current_procotol.file_size = 0;                                                        \
    current_procotol.from      = self->m_sock->getLocalAddress()->toString();

#define XX()                                                                               \
    c_chunk.f_name = current_procotol.file_name;                                           \
    c_chunk.index  = *( int* )current_procotol.customize[0];                               \
    for ( size_t i = 0; i < current_procotol.path.size(); i++ )                            \
    {                                                                                      \
        c_chunk.f_path += current_procotol.path[i] + "/";                                  \
    }                                                                                      \
    c_chunk.data       = ( char* )current_procotol.customize[0];                           \
    c_chunk.chunk_size = strlen( c_chunk.data );                                           \
    if ( c_chunk.chunk_size > self->max_chunk_size )                                       \
    {                                                                                      \
        ERROR_STD_STREAM_LOG( self->m_logger ) << "File Chunk is too large."               \
                                               << "%n%0";                                  \
        self->m_status = ERROR;                                                            \
        TT();                                                                              \
        current_procotol.bit = 4;                                                          \
        current_procotol.data.clear();                                                     \
        current_procotol.data.push_back( "ERROR" );                                        \
        YY();                                                                              \
        return;                                                                            \
    }

        /* 标识为1，读chunk */
        case 1:
            XX();
            self->get_chunk( c_chunk, temp );
            current_procotol.bit = 1;
            current_procotol.customize.clear();
            current_procotol.data.clear();
            current_procotol.data.push_back( temp );
            current_procotol.file_size = c_chunk.chunk_size;
            current_procotol.file_name = c_chunk.f_name;
            current_procotol.from      = self->m_sock->getLocalAddress()->toString();
            temp                       = c_chunk.f_path.c_str();

            /* 分割路径字符串 */
            while ( *temp )
            {
                if ( *temp == '/' )
                {
                    current_procotol.path.push_back( temp2 );
                    temp2.clear();
                }
                else
                {
                    temp2.push_back( *temp );
                    temp++;
                }
            }
            if ( !temp2.empty() )
            {
                current_procotol.path.push_back( temp2 );
                temp2.clear();
            }

            YY();
            break;
        /* 标识为2，写chunk */
        case 2:
            XX();
            self->write_chunk( c_chunk );
            TT();
            YY();
            break;
        /* 标识为3，删chunk */
        case 3:
            XX();
            self->delete_chunk( c_chunk );
            TT();
            YY();
            break;
        /* 标识为4， 向 master server 发送 chunk 的元数据 */
        case 4:
            /* 读全部的元数据 */
            self->get_all_meta_data( temp_arr );
            /* 把元数据转换为通信协议结构体 */
            current_procotol.bit       = 1;
            current_procotol.file_name = "";
            current_procotol.file_size = 0; /* 此时file_size代表，自定义数组的 gap */
            current_procotol.data.clear(); /* 存 chunk 的数据 */
            current_procotol.path.clear(); /* 存文件路径 */
            for ( size_t i = 0; i < temp_arr.size(); i++ )
            {
                current_procotol.data.push_back( temp_arr[i].data );
                current_procotol.path.push_back( temp_arr[i].f_path );
                current_procotol.customize.push_back( ( int* )temp_arr[i].index );
                *temp3 = temp_arr[i].f_name;
                current_procotol.customize.push_back( temp3 );
                current_procotol.customize.push_back( ( int* )strlen(temp_arr[i].data) );
            }
            current_procotol.from = self->m_sock->getLocalAddress()->toString();
            /* 发送数据 */
            YY();
            break;
        /* 标识为5， 关闭服务器 */
        case 5:
            self->close();
            break;
        default:
            ERROR_STD_STREAM_LOG( self->m_logger ) << "Unknown server instruction！"
                                                   << "%n%0";
            break;

#undef TT
#undef XX
#undef YY
    }
}

void chunk_server::close()
{
    /* 退出程序 */
    FATAL_STD_STREAM_LOG( this->m_logger ) << "Server exit！"
                                           << "%n%0";
    this->m_status = BREAKDOWN;
    exit( -1 );
}

void chunk_server::get_chunk( chunk_meta_data c_chunk, const char*& data )
{
    /* 拼出 chunk 的 key 值 */
    std::string key = c_chunk.f_path + "%" + c_chunk.f_name + "%" + std::to_string( c_chunk.index );
    std::string value;
    this->m_db->Get( key, value );
    data = value.c_str();
}

void chunk_server::write_chunk( chunk_meta_data c_chunk )
{
    /* 拼出 chunk 的 key 值 */
    std::string key = c_chunk.f_path + "%" + c_chunk.f_name + "%" + std::to_string( c_chunk.index );
    this->m_db->Put( key, c_chunk.data ); /* 写入数据 */
}

void chunk_server::delete_chunk( chunk_meta_data c_chunk )
{
    /* 拼出 chunk 的 key 值 */
    std::string key = c_chunk.f_path + "%" + c_chunk.f_name + "%" + std::to_string( c_chunk.index );
    this->m_db->Delete( key );
}

void chunk_server::get_all_meta_data( std::vector< chunk_meta_data >& arr )
{
    /* 遍历leveldb数据库，取出全部的元数据，发送给 master server */
    leveldb::DB* cur;
    this->m_db->get_leveldbObj( cur );
    leveldb::Iterator* it = cur->NewIterator( leveldb::ReadOptions() );
    for ( it->SeekToFirst(); it->Valid(); it->Next() )
    {
        // std::cout << it->key().ToString() << ": " << it->value().ToString() << std::endl;
        /* 分割 key */
        std::string temp = it->key().ToString();
        std::string buf  = "";
        int i            = 0;
        int j            = 0;
        chunk_meta_data current;
        while ( i < temp.size() )
        {
            if ( temp[i] == '%' )
            {
                switch ( j )
                {
                    case 0:
                        current.f_path = buf;
                        break;
                    case 1:
                        current.f_name = buf;
                        break;
                    case 2:
                        current.index = std::stoi( buf );
                        break;
                    default:
                        break;
                }
                j++;
                buf.clear();
                i++;
            }
            else
            {
                buf.push_back( temp[i] );
                i++;
            }
        }
        current.data = it->value().ToString().c_str();
        arr.push_back( current );
    }
}
}

#include "../../modules/setting/setting.cc"