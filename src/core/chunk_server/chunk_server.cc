#include "./chunk_server.h"
#include "core/tcpServer/tcpserver.h"
#include "modules/log/log.h"
#include "modules/socket/address.h"
#include <cstddef>
#include <string>
#include <unistd.h>

namespace star
{

static Threading::ptr chunk_server_thread;    /* 服务器线程 */
static Scheduler::ptr chunk_server_scheduler; /* 调度器 */
void* c_args;

chunk_server::chunk_server( std::filesystem::path settins_path )
: tcpserver( settins_path )
{
    m_master.addr = this->m_settings->get( "master_server" )["address"].asString();
    m_master.port = this->m_settings->get( "master_server" )["port"].asInt();
    print_logo();
    this->m_db->open();
}

void chunk_server::respond()
{
    try
    {
        /* 获取this指针 */
        chunk_server* self = ( chunk_server* )arg_ss.top();

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

        // INFO_STD_STREAM_LOG( self->m_logger ) << current_procotol->get_protocol_struct().file_name << "%n%0";

        /* switch 中 用到的变量 */
        chunk_meta_data got_chunk;
        std::vector< chunk_meta_data > arr;
        int port = self->m_settings->get( "server" )["port"].asInt();
        std::string file_name;
        std::string file_path;
        std::string file_new_path;
        std::string data;
        size_t index;
        const char* c_data;
        star::IPv4Address::ptr addr;
        star::MSocket::ptr sock;
    UPDO:

        switch ( cur.bit )
        {
            /* 取出全部的chunk数据 */
            case 106:
                self->updo_ss.push( cur );
                self->get_all_meta_data( arr );
                cur.bit  = 102;
                cur.from = self->m_sock->getLocalAddress()->toString();
                cur.path = std::to_string( port );
                for ( auto item : arr )
                {
                    cur.customize.push_back( item.f_name );
                    cur.customize.push_back( item.f_path );
                    cur.customize.push_back( std::to_string( item.chunk_size ) );
                    cur.customize.push_back( std::to_string( item.index ) );
                }

                /* 发送数据 */
                DEBUG_STD_STREAM_LOG( self->m_logger )
                << remote_sock->getRemoteAddress()->toString() << "%n%0";

                sleep(5);
                tcpserver::send( remote_sock, cur );
                break;

            /* 接受 chunk server 发来的 chunk 数据 */
            case 108:
                self->updo_ss.push( cur );
                got_chunk.f_name     = cur.file_name;
                got_chunk.f_path     = cur.path;
                got_chunk.data       = cur.data.c_str();
                got_chunk.index      = std::stoi( cur.customize[0] );
                got_chunk.chunk_size = cur.data.size();
                got_chunk.from       = self->m_sock->getLocalAddress()->toString();
                got_chunk.port       = port;
                self->write_chunk( got_chunk );

                cur.clear();
                cur.bit  = 116;
                cur.from = self->m_sock->getLocalAddress()->toString();
                cur.data = "true";

                tcpserver::send( remote_sock, cur );

                break;

            /* 修改块的路径 */
            case 122:
                self->updo_ss.push( cur );
                file_name        = cur.file_name;
                file_path        = cur.path;
                file_new_path    = cur.customize[0];
                index            = std::stoi( cur.customize[1] );
                got_chunk.f_name = file_name;
                got_chunk.f_path = file_path;
                got_chunk.index  = index;

                self->get_chunk( got_chunk, data );
                got_chunk.data       = data.c_str();
                got_chunk.chunk_size = data.size();
                got_chunk.port       = port;
                got_chunk.from       = self->m_sock->getLocalAddress()->toString();

                /* 重新写一个键值 */
                self->write_chunk( got_chunk );

                cur.clear();
                cur.bit       = 124;
                cur.file_name = file_name;
                cur.path      = file_new_path;
                cur.data      = "true";

                tcpserver::send( remote_sock, cur );

                break;

            /* 删除块的数据 */
            case 113:

                self->updo_ss.push( cur );

                got_chunk.f_name = cur.file_name;
                got_chunk.f_path = cur.path;
                got_chunk.index  = std::stoi( cur.customize[0] );

                self->delete_chunk( got_chunk );

                /* 更新 master server 的 数据 */
                self->get_all_meta_data( arr );
                cur.bit  = 102;
                cur.from = self->m_sock->getLocalAddress()->toString();
                cur.path = std::to_string( port );
                for ( auto item : arr )
                {
                    cur.customize.push_back( item.f_name );
                    cur.customize.push_back( item.f_path );
                    cur.customize.push_back( std::to_string( item.chunk_size ) );
                    cur.customize.push_back( std::to_string( item.index ) );
                }

                addr
                = star::IPv4Address::Create( self->m_master.addr.c_str(), self->m_master.port );
                sock = star::MSocket::CreateTCP( addr );

                if ( !sock->connect( addr ) )
                {
                    return;
                }
                tcpserver::send( sock, cur );

                cur.clear();

                /* 发送数据 */
                cur.bit       = 113;
                cur.file_name = file_name;
                cur.path      = file_path;
                cur.data      = "true";
                tcpserver::send( remote_sock, cur );
                break;

            /* 给客户端发送块数据 */
            case 110:
                got_chunk.f_name = cur.file_name;
                got_chunk.f_path = cur.path;
                got_chunk.index  = std::stoi( cur.customize[0] );

                self->get_chunk( got_chunk, data );

                cur.clear();

                cur.bit       = 115;
                cur.file_name = got_chunk.f_name;
                cur.path      = got_chunk.f_path;
                cur.customize.push_back( std::to_string( got_chunk.index ) );

                tcpserver::send( remote_sock, cur );

                break;

            /* 取出栈顶的协议结构体，重新执行上一操作 */
            case 123:
                cur = self->updo_ss.top();
                /* 返回到上面的switch */
                goto UPDO;
                break;

            default:
                break;
        }
    }
    catch ( std::exception& e )
    {
        throw e.what();
    }
}

void chunk_server::get_chunk( chunk_meta_data c_chunk, std::string& data )
{
    /* 拼出 chunk 的 key 值 */
    std::string key = c_chunk.f_path + "%" + c_chunk.f_name + "%" + std::to_string( c_chunk.index );
    this->m_db->Get( key, data );
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
        // current.data = it->value().ToString().c_str();
        arr.push_back( current );
    }
}

}

#include "../../modules/setting/setting.cc"