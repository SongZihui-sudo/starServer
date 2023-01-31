#include "./tcpserver.h"
#include "../../modules/common/common.h"
#include "modules/Scheduler/scheduler.h"
#include "modules/db/database.h"
#include "modules/log/log.h"
#include "modules/protocol/protocol.h"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>

namespace star
{
std::stack< void* > arg_ss;
std::stack< MSocket::ptr > sock_ss;
static Logger::ptr g_logger( STAR_NAME( "global_logger" ) );

levelDB::ptr tcpserver::m_db  = nullptr;
size_t tcpserver::buffer_size = 0;

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

    int max_services = this->m_settings->get( "max_services" ).asInt();
    this->m_service_manager.reset( new service_manager( max_services ) );
}

void tcpserver::wait( void respond(), void* self )
{
    /* 运行调度器 */
    server_scheduler.reset( new Scheduler( this->max_connects, this->max_connects ) );

    /* 监听 socket 连接 */
    this->m_status = Normal;

    /* 设置 this 指针 */
    arg_ss.push( self );

    /* 新建一个线程进行等待*/
    int index = 0;
    int i     = 0;
    this->m_sock->listen( this->max_connects );

    /* 阻塞线程，进行等待 */
    INFO_STD_STREAM_LOG( g_logger ) << std::to_string( getTime() ) << " <----> "
                                    << "Server Start Listening!"
                                    << "%n%0";

    while ( true )
    {
        remote_sock = this->m_sock->accept();
        if ( remote_sock )
        {
            sock_ss.push( remote_sock );
            this->connect_counter++;

            INFO_STD_STREAM_LOG( g_logger )
            << std::to_string( getTime() ) << " <----> "
            << "New Connect Form:" << this->m_sock->getRemoteAddress()->toString() << "%n%0";

            std::string thread_name = "respond" + std::to_string( index );

            /* 把新连接放入到调度器里 */
            server_scheduler->Regist_task( std::function< void() >( respond ), thread_name );
            server_scheduler->manage();

            index++;
        }
    }
}

void tcpserver::close()
{
    WERN_STD_STREAM_LOG( g_logger ) << "Server Exit, Code -1"
                                    << "%n%0";
    exit( -1 );
}

protocol::ptr tcpserver::recv( MSocket::ptr remote_sock, size_t buffer_size )
{
    protocol::Protocol_Struct ret;
    protocol::ptr protocoler( new protocol( "recv", ret ) );
    protocoler->Serialize();

    /* 初始化缓冲区 */
    char* buffer = new char[buffer_size];
    remote_sock->recv( buffer, buffer_size );

    std::string ready = buffer;

    if ( ready.empty() )
    {
        return nullptr;
    }

    /* 把缓冲区中读到的字符转换成为json格式 */
    if ( !protocoler->toJson( ready ) )
    {
        return nullptr;
    }

    /* 把 json 反序列化成为结构体 */
    protocoler->Deserialize();

    INFO_STD_STREAM_LOG( g_logger )
    << "****************** Get Message " << S( getTime() ) << " ******************"
    << "%n%0";
    protocoler->display();
    INFO_STD_STREAM_LOG( g_logger ) << "*************************************************"
                                    << "%n%0";

    delete[] buffer;

    return protocoler;
}

int tcpserver::send( MSocket::ptr remote_sock, protocol::Protocol_Struct buf )
{

    protocol::ptr protocoler( new protocol( "send", buf ) );

    /* 序列化结构体 */
    protocoler->set_protocol_struct( buf );
    protocoler->Serialize();

    /* 获得序列化后的字符串 */
    std::string buffer = protocoler->toStr();

    int flag = remote_sock->send( buffer.c_str(), buffer.size() );

    INFO_STD_STREAM_LOG( g_logger )
    << "****************** Send Message " << S( getTime() ) << " ******************"
    << "%n%0";
    protocoler->display();
    INFO_STD_STREAM_LOG( g_logger ) << "*************************************************"
                                    << "%n%0";

    return flag;
}

void tcpserver::bind()
{
    const char* addr = new char[100];
    strcpy( const_cast< char* >( addr ), this->m_settings->get( "server" )["address"].asCString() );

    int port     = this->m_settings->get( "server" )["port"].asInt();
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
        ERROR_STD_STREAM_LOG( g_logger ) << "Unknown address type！"
                                         << "%n%0";
        return;
    }

    /* 释放内存 */
    delete[] addr;
}

}