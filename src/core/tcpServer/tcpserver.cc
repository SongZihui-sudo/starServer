#include "./tcpserver.h"
#include "../../modules/common/common.h"
#include "modules/Scheduler/scheduler.h"
#include "modules/log/log.h"
#include "modules/protocol/protocol.h"

#include <exception>
#include <filesystem>
#include <functional>

namespace star
{

std::stack< void* > arg_ss;
std::stack< MSocket::ptr > sock_ss;

tcpserver::tcpserver( std::filesystem::path settings_path )
{
    /* 读取配置 */
    star::config::ptr Settings( new star::config( settings_path ) );
    this->m_settings = Settings;

    /* 最大的连接数 */
    this->max_connects = this->m_settings->get( "Max_connects" ).asInt();

    /* 缓冲区的默认大小 */
    this->buffer_size = this->m_settings->get( "BUffer_size" ).asInt();

    /* 初始化协议序列化器 */
    protocol::Protocol_Struct t;
    this->m_protocol.reset( new protocol( "Server", t ) );

    /* 初始化数据库 */
    std::string db_path = this->m_settings->get( "DBpath" ).asString();
    std::string db_name = this->m_settings->get( "DBname" ).asString();
    this->m_db.reset( new star::levelDB( db_name, db_path ) );

    this->m_name = this->m_settings->get( "ServerName" ).asString();

    this->m_logger.reset( STAR_NAME( "MASTER_SERVER_LOGGER" ) );

    const char* addr = new char[100];
    strcpy( const_cast< char* >( addr ),
            this->m_settings->get( "master_server" )["address"].asCString() );

    int port     = this->m_settings->get( "master_server" )["port"].asInt();
    this->m_sock = MSocket::CreateTCPSocket(); /* 创建一个TCP Socket */

    if ( this->m_settings->get( "AddressType" ).asString() == "IPv4" )
    {
        IPv4Address::ptr temp( new IPv4Address() );
        temp = IPv4Address::Create( addr, port );
        this->m_sock->bind( temp ); /* 绑定地址 */
    }
    else if ( this->m_settings->get( "AddressType" ).asString() == "IPv6" )
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

    /* 释放内存 */
    delete[] addr;
}

void tcpserver::wait( void respond(), void* self )
{
    /* 运行调度器 */
    server_scheduler.reset( new Scheduler( this->max_connects, this->max_connects ) );
    server_scheduler->run(); /* 等待进行任务调度 */

    /* 监听 socket 连接 */
    this->m_status = Normal;

    /* 设置 this 指针 */
    arg_ss.push( self );

    /* 新建一个线程进行等待*/
    int index = 0;
    int i     = 0;
    this->m_sock->listen( this->max_connects );

    MESSAGE_MAP( SOCKET_CONNECTS, this->max_connects );

    /* 阻塞线程，进行等待 */
    INFO_STD_STREAM_LOG( this->m_logger ) << std::to_string( getTime() ) << " <----> "
                                          << "Server Start Listening!"
                                          << "%n%0";

    while ( true )
    {
        remote_sock = this->m_sock->accept();
        if ( remote_sock )
        {
            sock_ss.push( remote_sock );

            INFO_STD_STREAM_LOG( this->m_logger )
            << std::to_string( getTime() ) << " <----> "
            << "New Connect Form:" << this->m_sock->getRemoteAddress()->toString() << "%n%0";

            std::string thread_name = "respond" + std::to_string( index );

            /* 把新连接放入到调度器里 */
            server_scheduler->Regist_task(std::function< void() >( respond ),thread_name);
            server_scheduler->manage();

            index++;
        }
    }
}

void tcpserver::close()
{
    WERN_STD_STREAM_LOG( this->m_logger ) << "Server Exit, Code -1"
                                          << "%n%0";
    exit( -1 );
}

protocol::Protocol_Struct tcpserver::recv( MSocket::ptr remote_sock )
{
    protocol::Protocol_Struct ret;

    /* 初始化缓冲区 */
    char* buffer = new char[this->buffer_size];
    remote_sock->recv( buffer, this->buffer_size );

    /* 把缓冲区中读到的字符转换成为json格式 */
    this->m_protocol->toJson( buffer );

    /* 把 json 反序列化成为结构体 */
    this->m_protocol->set_protocol_struct( ret );
    this->m_protocol->Deserialize();

    delete[] buffer;
    return ret;
}

int tcpserver::send( MSocket::ptr remote_sock, protocol::Protocol_Struct buf )
{
    /* 序列化结构体 */
    this->m_protocol->set_protocol_struct( buf );
    this->m_protocol->Serialize();

    /* 获得序列化后的字符串 */
    const char* buffer = new char[this->buffer_size];
    this->m_protocol->toCStr( buffer );

    int flag = remote_sock->send( buffer, this->buffer_size );

    delete[] buffer;
    return flag;
}

}