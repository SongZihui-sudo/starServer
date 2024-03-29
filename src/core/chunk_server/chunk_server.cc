#include "./chunk_server.h"
#include "core/tcpServer/tcpserver.h"
#include "modules/log/log.h"
#include "modules/meta_data/chunk.h"
#include "modules/meta_data/file.h"
#include "modules/meta_data/meta_data.h"

#include <bits/types/FILE.h>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <sstream>
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
    this->m_master.addr  = this->m_settings->get( "master_server" )["address"].asString();
    this->m_master.port  = this->m_settings->get( "master_server" )["port"].asInt();
    this->max_chunk_size = this->m_settings->get( "chunk_size" ).asInt64();
    print_logo();
    this->m_db->open();
}

void chunk_server::respond()
{
    try
    {
        /* 获取this指针 */
        chunk_server* self = ( chunk_server* )arg_ss.top();

        if ( !self )
        {
            return;
        }

        MSocket::ptr& remote_sock = sock_ss.front();

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
    size_t index = std::stoi( cur.customize[0] );
    std::string buffer;
    bool bit = true;
    /* 读，判断这个块是否存在 */
    cur_file->open( file_operation::read, self->m_db ); /* 打开文件 */
    cur_file->read( buffer, index );
    cur_file->close(); /* 关闭文件 */
    /* 存在 */
    if ( !buffer.empty() )
    {
        buffer += '|' + cur.data; /* chunk 内的每个package以‘|’隔开 */
        cur_file->open( file_operation::write, self->m_db ); /* 打开文件 */
        bit = cur_file->write( buffer, index );
        cur_file->close();
    }
    else
    {
        /* 不存在 */
        cur_file->open( file_operation::write, self->m_db ); /* 打开文件 */
        bit = cur_file->append( cur.data );                  /* 写文件, 追加 */
        cur_file->close();                                   /* 关闭文件 */
    }

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
    MSocket::ptr remote_sock;
    remote_sock.reset( ( MSocket* )args[3] );
    
    int port         = self->m_settings->get( "server" )["port"].asInt();
    std::string flag = "true";

    std::string file_new_path = cur.customize[0];
    file::ptr cur_file( new file( cur.file_name, cur.path, self->max_chunk_size ) );
    cur_file->open( file_operation::write, self->m_db ); /* 打开文件 */
    bool bit = cur_file->move( file_new_path );          /* 移动文件 */
    cur_file->close();                                   /* 关闭文件 */

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
    MSocket::ptr remote_sock;
    remote_sock.reset( ( MSocket* )args[3] );

    BREAK( g_logger );
    std::string flag = "true";
    file::ptr cur_file( new file( cur.file_name, cur.path, self->max_chunk_size ) );
    cur_file->open( file_operation::write, self->m_db ); /* 打开文件 */
    size_t index = std::stoi( cur.customize[0] );
    bool bit     = cur_file->del( index );
    cur_file->close(); /* 关闭文件 */
    BREAK( g_logger );
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
    MSocket::ptr remote_sock;
    remote_sock.reset( ( MSocket* )args[3] );

    if ( cur.file_name.empty() || cur.path.empty() )
    {
        return;
    }

    file::ptr cur_file( new file( cur.file_name, cur.path, self->max_chunk_size ) );

    if ( cur.customize.size() < 2 )
    {
        cur.reset( 115,
                   self->m_sock->getLocalAddress()->toString(),
                   cur_file->get_name(),
                   cur_file->get_path(),
                   data.size(),
                   "error!",
                   { S( 0 ) } );
        tcpserver::send( remote_sock, cur );
        return;
    }

    size_t index               = std::stoi( cur.customize[0] );
    size_t start_package_index = std::stoi( cur.customize[1] );

    cur_file->open( file_operation::read, self->m_db );
    bool read_flag = cur_file->read( data, index );
    cur_file->close();

    std::stringstream datas( data );
    data.clear();

    /* 跳到客户端请求的package开始位置 */
    while ( start_package_index )
    {
        std::string temp_data;
        std::getline( datas, temp_data, '|' );
        start_package_index--;
    }

    /* 把一个chunk划分为若干个package,发给客户端 */
    std::string temp_data;
    while ( std::getline( datas, temp_data, '|' ) )
    {
        DEBUG_STD_STREAM_LOG( g_logger ) << "Package Data: " << temp_data << Logger::endl();
        if ( temp_data.size() > self->m_package_size )
        {
            temp_data.resize( self->m_package_size );
        }

        /* 回复消息 */
        cur.reset( 115,
                   self->m_sock->getLocalAddress()->toString(),
                   cur_file->get_name(),
                   cur_file->get_path(),
                   temp_data.size(),
                   temp_data,
                   { S( index ) } );
        // BREAK( g_logger );
        tcpserver::send( remote_sock, cur );
        protocol::ptr cur_ptotocol = tcpserver::recv( remote_sock, self->buffer_size );
        if ( !cur_ptotocol )
        {
            FATAL_STD_STREAM_LOG( g_logger ) << "Get Message Error!" << Logger::endl();
            break;
        }
        cur = cur_ptotocol->get_protocol_struct();
        if ( cur.bit == 141 && cur.data == "recv data ok" )
        {
            INFO_STD_STREAM_LOG( g_logger ) << "%D"
                                            << "Client Recv Data OK!" << Logger::endl();
        }
        else
        {
            FATAL_STD_STREAM_LOG( g_logger ) << "Client Recv Data Error!" << Logger::endl();
            break;
        }
    }
    data = "chunk end";
    cur.reset( 115,
               self->m_sock->getLocalAddress()->toString(),
               cur_file->get_name(),
               cur_file->get_path(),
               data.size(),
               data,
               { S( index ) } );
    BREAK( g_logger );
    tcpserver::send( remote_sock, cur );
}

/*
    块重命名
*/
void chunk_server::deal_with_137( std::vector< void* > args )
{
    chunk_server* self            = ( chunk_server* )args[0];
    protocol::Protocol_Struct cur = *( protocol::Protocol_Struct* )args[1];
    MSocket::ptr remote_sock;
    remote_sock.reset( ( MSocket* )args[3] );

    std::string reply = "true";

    std::string file_name = cur.file_name;
    std::string file_path = cur.path;
    std::string new_name = cur.customize[0];

    file::ptr fp( new file( file_name, file_path, self->max_chunk_size ) );
    fp->open( file_operation::write, self->m_db );
    if ( !fp->rename( new_name ) )
    {
        reply = "false";
    }
    fp->close();
    cur.reset( 139, self->m_sock->getLocalAddress()->toString(), new_name, file_path, reply.size(), reply, {} );
    tcpserver::send( remote_sock, cur );
    BREAK(g_logger);
}

}