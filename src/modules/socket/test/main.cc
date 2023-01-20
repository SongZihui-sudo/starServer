#include "../../common/common.h"
#include "../../log/log.h"
#include "../socket.h"
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
        INFO_STD_STREAM_LOG( g_logger ) << "get address: " << addr->toString() << "%n%0";
    }
    else
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "get address fail"
                                         << "%n%0";
        return;
    }

    star::MSocket::ptr sock = star::MSocket::CreateTCP( addr );
    addr->setPort( 80 );
    INFO_STD_STREAM_LOG( g_logger ) << "addr=" << addr->toString() << "%n%0";
    if ( !sock->connect( addr ) )
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "connect " << addr->toString() << " fail"
                                         << "%n%0";
        return;
    }
    else
    {
        INFO_STD_STREAM_LOG( g_logger ) << "connect " << addr->toString() << " connected"
                                        << "%n%0";
    }

    const char buff[] = "GET / HTTP/1.0\r\n\r\n";
    int rt            = sock->send( buff, sizeof( buff ) );
    if ( rt <= 0 )
    {
        INFO_STD_STREAM_LOG( g_logger ) << "send fail rt=" << S( rt ) << "%n%0";
        return;
    }

    std::string buffs;
    buffs.resize( 4096 );
    rt = sock->recv( &buffs[0], buffs.size() );

    if ( rt <= 0 )
    {
        INFO_STD_STREAM_LOG( g_logger ) << "recv fail rt=" << S( rt ) << "%n%0";
        return;
    }

    buffs.resize( rt );
    INFO_STD_STREAM_LOG( g_logger ) << buffs << "%n%0";
}

void test2()
{
    star::IPAddress::ptr addr = star::Address::LookupAnyIPAddress( "www.baidu.com:80" );
    if ( addr )
    {
        INFO_STD_STREAM_LOG( g_logger ) << "get address: " << addr->toString() << "%n%0";
    }
    else
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "get address fail"
                                         << "%n%0";
        ;
        return;
    }

    star::MSocket::ptr sock = star::MSocket::CreateTCP( addr );
    if ( !sock->connect( addr ) )
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "connect " << addr->toString() << " fail"
                                         << "%n%0";
        return;
    }
    else
    {
        INFO_STD_STREAM_LOG( g_logger ) << "connect " << addr->toString() << " connected"
                                        << "%n%0";
    }

    uint64_t ts = getTime();
    for ( size_t i = 0; i < 10000000000ul; ++i )
    {
        if ( int err = sock->getError() )
        {
            INFO_STD_STREAM_LOG( g_logger )
            << "err=" << S( err ) << " errstr=" << strerror( err ) << "%n%0";
            break;
        }

        static int batch = 10000000;
        if ( i && ( i % batch ) == 0 )
        {
            uint64_t ts2 = getTime();
            INFO_STD_STREAM_LOG( g_logger )
            << "i=" << S( i ) << " used: " << S( ( ts2 - ts ) * 1.0 / batch ) << " us"
            << "%n%0";
            ts = ts2;
        }
    }
}

/* socket 服务端 例子 */
void test3()
{
    /* ipv4 地址 */
    star::IPv4Address::ptr addr = star::IPv4Address::Create( "127.0.0.1", 7000 );
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
    << "server Start listening !" << m_socket->getLocalAddress()->toString() << "%n%0";
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
        << "Form:" << m_socket->getRemoteAddress()->toString() << "Msg:" << cur << "%n%0";

        /* 当消息为end的时候，停止监听 */
        if ( cur == "End" )
        {
            WERN_STD_STREAM_LOG( g_logger ) << "STOP LISTENING!"
                                            << "%n%0";
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