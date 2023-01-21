#include "./chunk_server.h"
#include "core/tcpServer/tcpserver.h"

namespace star
{

static Threading::ptr chunk_server_thread;    /* 服务器线程 */
static Scheduler::ptr chunk_server_scheduler; /* 调度器 */
void* c_args;

chunk_server::chunk_server( std::filesystem::path settins_path )
: tcpserver( settins_path )
{
    /* 进行设置 */
    const char* addr = this->m_settings->get( "IPAddress" ).asCString();
    int port         = this->m_settings->get( "Port" ).asInt();
    this->m_sock     = MSocket::CreateTCPSocket(); /* 创建一个TCP Socket */
    if ( this->m_settings->get( "AddressType" ).asString() == "IPv4" )
    {
        IPv4Address::ptr temp( new IPv4Address() );
        this->m_sock->bind( temp ); /* 绑定地址 */
    }
    else if ( this->m_settings->get( "AddressType" ).asString() == "IPv6" )
    {
        IPv6Address::ptr temp( new IPv6Address() );
        this->m_sock->bind( temp ); /* 绑定地址 */
    }

    this->m_status = INIT;

    std::string db_path = this->m_settings->get( "DBpath" ).asString();
    std::string db_name = this->m_settings->get( "DBname" ).asString();
    this->m_db.reset( new star::levelDB( db_name, db_path ) ); /* 初始化数据库 */

    this->m_name = this->m_settings->get( "ServerName" ).asString();

    this->m_logger.reset( STAR_NAME( "CHUNK_SERVER_LOGGER" ) );
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
        default:
            ERROR_STD_STREAM_LOG( self->m_logger ) << "Unknown server instruction！"
                                                   << "%n%0";
            break;

#undef TT
#undef XX
#undef YY
    }
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