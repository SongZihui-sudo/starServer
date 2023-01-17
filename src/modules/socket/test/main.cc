#include "../socket.h"
#include "../../common/common.h"
#include "../../log/log.h"

star::Logger::ptr g_logger( STAR_NAME( "system" ) );

void test_socket()
{
    star::IPAddress::ptr addr = star::Address::LookupAnyIPAddress( "cn.bing.com" );

    if ( addr )
    {
        INFO_STD_STREAM_LOG( g_logger ) << "get address: " << addr->toString() << "%n%0";
    }
    else
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "get address fail" << "%n%0";
        return;
    }

    star::MSocket::ptr sock = star::MSocket::CreateTCP( addr );
    addr->setPort( 80 );
    INFO_STD_STREAM_LOG( g_logger ) << "addr=" << addr->toString() << "%n%0";
    if ( !sock->connect( addr ) )
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "connect " << addr->toString() << " fail" << "%n%0";
        return;
    }
    else
    {
        INFO_STD_STREAM_LOG( g_logger ) << "connect " << addr->toString() << " connected" << "%n%0";
    }

    const char buff[] = "GET / HTTP/1.0\r\n\r\n";
    int rt            = sock->send( buff, sizeof( buff ) );
    if ( rt <= 0 )
    {
        INFO_STD_STREAM_LOG( g_logger ) << "send fail rt=" << S(rt) << "%n%0";
        return;
    }

    std::string buffs;
    buffs.resize( 4096 );
    rt = sock->recv( &buffs[0], buffs.size() );

    if ( rt <= 0 )
    {
        INFO_STD_STREAM_LOG( g_logger ) << "recv fail rt=" << S(rt) << "%n%0";
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
        ERROR_STD_STREAM_LOG( g_logger ) << "get address fail" << "%n%0";;
        return;
    }

    star::MSocket::ptr sock = star::MSocket::CreateTCP( addr );
    if ( !sock->connect( addr ) )
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "connect " << addr->toString() << " fail" << "%n%0";
        return;
    }
    else
    {
        INFO_STD_STREAM_LOG( g_logger ) << "connect " << addr->toString() << " connected" << "%n%0";
    }

    uint64_t ts = getTime();
    for ( size_t i = 0; i < 10000000000ul; ++i )
    {
        if ( int err = sock->getError() )
        {
            INFO_STD_STREAM_LOG( g_logger ) << "err=" << S(err) << " errstr=" << strerror( err ) << "%n%0";
            break;
        }

        static int batch = 10000000;
        if ( i && ( i % batch ) == 0 )
        {
            uint64_t ts2 = getTime();
            INFO_STD_STREAM_LOG( g_logger )
            << "i=" << S(i) << " used: " << S( ( ts2 - ts ) * 1.0 / batch ) << " us" << "%n%0";
            ts = ts2;
        }
    }
}

void test3()
{
    star::IPAddress::ptr addr = star::Address::LookupAnyIPAddress( "www.baidu.com:80" );
}

int main( int argc, char** argv )
{
    // iom.schedule(&test_socket);
    test_socket();
    return 0;
}