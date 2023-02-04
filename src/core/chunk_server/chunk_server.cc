#include "./chunk_server.h"
#include "core/tcpServer/tcpserver.h"
#include "modules/log/log.h"
#include "modules/meta_data/meta_data.h"
#include "modules/socket/socket.h"

#include <bits/types/FILE.h>
#include <cstddef>
#include <cstdint>
#include <star.h>
#include <string>
#include <unistd.h>

namespace star
{

static io_lock::ptr server_lock( new io_lock() );
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

        /* 执行响应的响应函数 */
        if ( cur.bit == -1 || !cur.bit )
        {
            return;
        }
        self->message_funcs[cur.bit]( { self, &cur, current_procotol.get(), remote_sock.get() } );
    }
    catch ( std::exception& e )
    {
        throw e.what();
    }
}

/*
    接受 master server 发来的 chunk 数据
*/
void chunk_server::deal_with_108( std::vector< void* > args )
{
    server_lock->lock_write( file_operation::write );
    chunk_server* self            = ( chunk_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    MSocket::ptr remote_sock;
    remote_sock.reset( ( MSocket* )args[3] );
    std::string flag = "true";
    file::ptr cur_file( new file( cur.file_name, cur.path, self->max_chunk_size ) );    
    cur_file->open( self->m_db );                          /* 打开文件 */
    bool bit = cur_file->append( file_operation::write, cur.data ); /* 写文件, 追加 */
    cur_file->close();                                              /* 关闭文件 */

    if ( !bit )
    {
        FATAL_STD_STREAM_LOG( g_logger ) << "%D"
                                         << "Append file data fail!" << Logger::endl();
        flag = "false";
    }
    INFO_STD_STREAM_LOG( g_logger ) << "%D"
                                    << "Append file data successfully!" << Logger::endl();

    cur.reset( 116, self->m_sock->getLocalAddress()->toString(), "", "", 0, flag, {} );
    tcpserver::send( remote_sock, cur );
    server_lock->release_write();
}

/*
    修改块的路径
*/
void chunk_server::deal_with_122( std::vector< void* > args )
{
    chunk_server* self            = ( chunk_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    int port                      = self->m_settings->get( "server" )["port"].asInt();
    std::string flag              = "true";

    std::string file_new_path = cur.customize[0];
    file::ptr cur_file( new file( cur.file_name, cur.path, self->max_chunk_size ) );
    cur_file->open( self->m_db );               /* 打开文件 */
    bool bit = cur_file->move( file_new_path ); /* 移动文件 */
    cur_file->close();                          /* 关闭文件 */

    if ( !bit )
    {
        flag = "false";
    }

    cur.reset( 124, "", "", file_new_path, 0, flag, {} );
    tcpserver::send( remote_sock, cur );
}

/*
    删除块的数据
*/
void chunk_server::deal_with_113( std::vector< void* > args )
{
    chunk_server* self            = ( chunk_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    std::string flag              = "true";
    int port                      = self->m_settings->get( "server" )["port"].asInt();
    file::ptr cur_file( new file( cur.file_name, cur.path, self->max_chunk_size ) );
    cur_file->open( self->m_db ); /* 打开文件 */
    size_t index = std::stoi( cur.customize[0] );
    bool bit     = cur_file->del( file_operation::write, index );
    cur_file->close(); /* 关闭文件 */

    if ( !bit )
    {
        flag = "false";
    }

    /* 发送数据 */
    cur.reset( 129,
               self->m_sock->getLocalAddress()->toString(),
               cur_file->get_name(),
               cur_file->get_path(),
               0,
               flag,
               {} );
    tcpserver::send( remote_sock, cur );
}

/*
    给客户端发送块数据
*/
void chunk_server::deal_with_110( std::vector< void* > args )
{
    chunk_server* self            = ( chunk_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    std::string data;
    std::string flag;
    file::ptr cur_file( new file( cur.file_name, cur.path, self->max_chunk_size ) );
    int index = std::stoi( cur.customize[0] );
    cur_file->open( self->m_db );
    cur_file->read( file_operation::read, data, index );
    cur_file->close();

    /* 回复消息 */
    cur.reset( 115,
               self->m_sock->getLocalAddress()->toString(),
               cur_file->get_name(),
               cur_file->get_path(),
               0,
               data,
               { S( index ) } );

    tcpserver::send( remote_sock, cur );
}

}