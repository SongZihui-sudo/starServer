#include "../../common/common.h"
#include "../../log/log.h"
#include "../socket.h"
#include "../../protocol/protocol.h"

#include "modules/socket/address.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

star::Logger::ptr g_logger( STAR_NAME( "system" ) );

void test1()
{
    star::IPAddress::ptr addr = star::Address::LookupAnyIPAddress( "www.baidu.com" );

    if ( addr )
    {
        INFO_STD_STREAM_LOG( g_logger ) << "get address: " << addr->toString() << Logger::endl();
    }
    else
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "get address fail"
                                         << Logger::endl();
        return;
    }

    star::MSocket::ptr sock = star::MSocket::CreateTCP( addr );
    addr->setPort( 80 );
    INFO_STD_STREAM_LOG( g_logger ) << "addr=" << addr->toString() << Logger::endl();
    if ( !sock->connect( addr ) )
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "connect " << addr->toString() << " fail"
                                         << Logger::endl();
        return;
    }
    else
    {
        INFO_STD_STREAM_LOG( g_logger ) << "connect " << addr->toString() << " connected"
                                        << Logger::endl();
    }

    const char buff[] = "GET / HTTP/1.0\r\n\r\n";
    int rt            = sock->send( buff, sizeof( buff ) );
    if ( rt <= 0 )
    {
        INFO_STD_STREAM_LOG( g_logger ) << "send fail rt=" << S( rt ) << Logger::endl();
        return;
    }

    std::string buffs;
    buffs.resize( 4096 );
    rt = sock->recv( &buffs[0], buffs.size() );

    if ( rt <= 0 )
    {
        INFO_STD_STREAM_LOG( g_logger ) << "recv fail rt=" << S( rt ) << Logger::endl();
        return;
    }

    buffs.resize( rt );
    INFO_STD_STREAM_LOG( g_logger ) << buffs << Logger::endl();
}

void test2()
{
    star::IPAddress::ptr addr = star::Address::LookupAnyIPAddress( "www.baidu.com:80" );
    if ( addr )
    {
        INFO_STD_STREAM_LOG( g_logger ) << "get address: " << addr->toString() << Logger::endl();
    }
    else
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "get address fail"
                                         << Logger::endl();
        ;
        return;
    }

    star::MSocket::ptr sock = star::MSocket::CreateTCP( addr );
    if ( !sock->connect( addr ) )
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "connect " << addr->toString() << " fail"
                                         << Logger::endl();
        return;
    }
    else
    {
        INFO_STD_STREAM_LOG( g_logger ) << "connect " << addr->toString() << " connected"
                                        << Logger::endl();
    }

    uint64_t ts = getTime();
    for ( size_t i = 0; i < 10000000000ul; ++i )
    {
        if ( int err = sock->getError() )
        {
            INFO_STD_STREAM_LOG( g_logger )
            << "err=" << S( err ) << " errstr=" << strerror( err ) << Logger::endl();
            break;
        }

        static int batch = 10000000;
        if ( i && ( i % batch ) == 0 )
        {
            uint64_t ts2 = getTime();
            INFO_STD_STREAM_LOG( g_logger )
            << "i=" << S( i ) << " used: " << S( ( ts2 - ts ) * 1.0 / batch ) << " us"
            << Logger::endl();
            ts = ts2;
        }
    }
}

/* socket 服务端 例子 */
void test3()
{
    /* ipv4 地址 */
    star::IPv4Address::ptr addr = star::IPv4Address::Create( "192.168.0.103", 7000 );
    /* 创建一个 socket */
    star::MSocket::ptr m_socket = star::MSocket::CreateTCP( addr );
    /* 绑定地址 */
    if ( !m_socket->bind( addr ) )
    {
        return;
    }
    /* 监听连接 */
    if ( !m_socket->listen( 5 ) )
    {
        return;
    };
    DEBUG_STD_STREAM_LOG( g_logger )
    << "server Start listening !" << m_socket->getLocalAddress()->toString() << Logger::endl();
    star::MSocket::ptr client_addr = nullptr;

    client_addr = m_socket->accept();

    /* 阻塞当前的线程 */
    while ( true )
    {
        char* buffer = new char[1024];

        client_addr->recv( buffer, 1024 ); /* 接受消息 */

        std::string cur = buffer;

        INFO_STD_STREAM_LOG( g_logger )
        << "Get Message"
        << "%n"
        << "Form:" << m_socket->getRemoteAddress()->toString() << "Msg:" << cur << Logger::endl();

        star::protocol::Protocol_Struct t;
        star::protocol::ptr test(new star::protocol("test", t));

        test->toJson(cur);

        /* 当消息为end的时候，停止监听 */
        if ( cur == "End" )
        {
            WERN_STD_STREAM_LOG( g_logger ) << "STOP LISTENING!"
                                            << Logger::endl();
            client_addr->send( "End", 2 );
            delete[] buffer;
            break;
        }

        client_addr->send( "ok", 2 );

        delete[] buffer;
    }
}

int main( int argc, char** argv )
{
    // iom.schedule(&test_socket);
    // test_socket();
    test3();
    return 0;
}