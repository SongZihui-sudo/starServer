#include "./socket.h"
#include "../log/log.h"

#include <iostream>
#include <limits.h>

namespace star
{

static star::Logger::ptr g_logger( STAR_NAME( "system" ) );

MSocket::ptr MSocket::CreateTCP( star::Address::ptr address )
{
    MSocket::ptr sock( new MSocket( address->getFamily(), TCP, 0 ) );
    return sock;
}

MSocket::ptr MSocket::CreateUDP( star::Address::ptr address )
{
    MSocket::ptr sock( new MSocket( address->getFamily(), UDP, 0 ) );
    sock->newSock();
    sock->m_isConnected = true;
    return sock;
}

MSocket::ptr MSocket::CreateTCPSocket()
{
    MSocket::ptr sock( new MSocket( IPv4, TCP, 0 ) );
    return sock;
}

MSocket::ptr MSocket::CreateUDPSocket()
{
    MSocket::ptr sock( new MSocket( IPv4, UDP, 0 ) );
    sock->newSock();
    sock->m_isConnected = true;
    return sock;
}

MSocket::ptr MSocket::CreateTCPSocket6()
{
    MSocket::ptr sock( new MSocket( IPv6, TCP, 0 ) );
    return sock;
}

MSocket::ptr MSocket::CreateUDPSocket6()
{
    MSocket::ptr sock( new MSocket( IPv6, UDP, 0 ) );
    sock->newSock();
    sock->m_isConnected = true;
    return sock;
}

MSocket::ptr MSocket::CreateUnixTCPSocket()
{
    MSocket::ptr sock( new MSocket( UNIX, TCP, 0 ) );
    return sock;
}

MSocket::ptr MSocket::CreateUnixUDPSocket()
{
    MSocket::ptr sock( new MSocket( UNIX, UDP, 0 ) );
    return sock;
}

MSocket::MSocket( int family, int type, int protocol )
: m_sock( -1 )
, m_family( family )
, m_type( type )
, m_protocol( protocol )
, m_isConnected( false )
{
}

MSocket::~MSocket()
{
    if ( !this )
    {
        return;
    }
    close();
}

bool MSocket::getOption( int level, int option, void* result, socklen_t* len )
{
    int rt = getsockopt( m_sock, level, option, result, ( socklen_t* )len );
    if ( rt )
    {
        DEBUG_STD_STREAM_LOG( g_logger )
        << "getOption sock=" << std::to_string( m_sock ) << " level=" << std::to_string( level )
        << " option=" << std::to_string( option ) << " errno=" << std::to_string( errno )
        << " errstr=" << strerror( errno ) << Logger::endl();
        return false;
    }
    return true;
}

bool MSocket::setOption( int level, int option, const void* result, socklen_t len )
{
    if ( setsockopt( m_sock, level, option, result, ( socklen_t )len ) )
    {
        DEBUG_STD_STREAM_LOG( g_logger )
        << "getOption sock=" << std::to_string( m_sock ) << " level=" << std::to_string( level )
        << " option=" << std::to_string( option ) << " errno=" << std::to_string( errno )
        << " errstr=" << strerror( errno ) << Logger::endl();
        return false;
    }
    return true;
}

MSocket::ptr MSocket::accept()
{
    MSocket::ptr sock( new MSocket( m_family, m_type, m_protocol ) );
    int newsock = ::accept( m_sock, nullptr, nullptr );
    if ( newsock == -1 )
    {
        ERROR_STD_STREAM_LOG( g_logger )
        << "accept(" << std::to_string( m_sock ) << ") errno=" << std::to_string( errno )
        << " errstr=" << strerror( errno ) << Logger::endl();
        return nullptr;
    }
    if ( sock->init( newsock ) )
    {
        return sock;
    }
    return nullptr;
}

bool MSocket::init( int sock )
{
    m_sock        = sock;
    m_isConnected = true;
    initSock();
    getLocalAddress();
    getRemoteAddress();
    return true;
}

bool MSocket::bind( const Address::ptr addr )
{
    // m_localAddress = addr;
    if ( !isValid() )
    {
        newSock();
        if ( STAR_UNLIKELY( !isValid() ) )
        {
            return false;
        }
    }

    if ( STAR_UNLIKELY( addr->getFamily() != m_family ) )
    {
        ERROR_STD_STREAM_LOG( g_logger )
        << "bind sock.family(" << S( m_family ) << ") addr.family(" << S( addr->getFamily() )
        << ") not equal, addr=" << addr->toString() << Logger::endl();
        return false;
    }

    UnixAddress::ptr uaddr = std::dynamic_pointer_cast< UnixAddress >( addr );
    if ( uaddr )
    {
        MSocket::ptr sock = MSocket::CreateUnixTCPSocket();
        if ( sock->connect( uaddr ) )
        {
            return false;
        }
    }

    if ( ::bind( m_sock, addr->getAddr(), addr->getAddrLen() ) )
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "bind error errrno=" << S( errno )
                                         << " errstr=" << strerror( errno ) << Logger::endl();
        return false;
    }
    getLocalAddress();
    return true;
}

bool MSocket::reconnect( uint64_t timeout_ms )
{
    if ( !m_remoteAddress )
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "reconnect m_remoteAddress is null" << Logger::endl();
        return false;
    }
    m_localAddress.reset();
    return connect( m_remoteAddress, timeout_ms );
}

bool MSocket::connect( const Address::ptr addr, uint64_t timeout_ms )
{
    m_remoteAddress = addr;
    if ( !isValid() )
    {
        newSock();
        if ( STAR_UNLIKELY( !isValid() ) )
        {
            return false;
        }
    }

    // DEBUG_STD_STREAM_LOG(g_logger) << m_remoteAddress->toString() << Logger::endl();

    if ( STAR_UNLIKELY( addr->getFamily() != m_family ) )
    {
        ERROR_STD_STREAM_LOG( g_logger )
        << "connect sock.family(" << S( m_family ) << ") addr.family(" << S( addr->getFamily() )
        << ") not equal, addr=" << addr->toString() << Logger::endl();
        return false;
    }

    if ( timeout_ms == ( uint64_t )-1 )
    {
        // DEBUG_STD_STREAM_LOG(g_logger) << addr->toString() << Logger::endl();
        if ( ::connect( m_sock, addr->getAddr(), addr->getAddrLen() ) )
        {
            ERROR_STD_STREAM_LOG( g_logger )
            << "sock=" << S( m_sock ) << " connect(" << addr->toString()
            << ") error errno=" << S( errno ) << " errstr=" << strerror( errno ) << Logger::endl();
            close();
            return false;
        }
    }
    else
    {
        ERROR_STD_STREAM_LOG( g_logger )
        << "sock=" << S( m_sock ) << " connect(" << addr->toString()
        << " error errno=" << S( errno ) << " errstr=" << strerror( errno );
        close();
        return false;
    }

    m_isConnected = true;
    getRemoteAddress();
    getLocalAddress();
    return true;
}

bool MSocket::listen( int backlog )
{
    if ( !isValid() )
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "listen error sock=-1" << Logger::endl();
        return false;
    }
    if ( ::listen( m_sock, backlog ) )
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "listen error errno=" << S( errno )
                                         << " errstr=" << strerror( errno ) << Logger::endl();
        return false;
    }
    return true;
}

bool MSocket::close()
{
    if ( !m_isConnected && m_sock == -1 )
    {
        return true;
    }
    m_isConnected = false;
    if ( m_sock != -1 )
    {
        ::close( m_sock );
        m_sock = -1;
    }
    return false;
}

int MSocket::send( const void* buffer, size_t length, int flags )
{
    if ( isConnected() )
    {
        return ::send( m_sock, buffer, length, flags );
    }
    return -1;
}

int MSocket::send( const iovec* buffers, size_t length, int flags )
{
    if ( isConnected() )
    {
        msghdr msg;
        memset( &msg, 0, sizeof( msg ) );
        msg.msg_iov    = ( iovec* )buffers;
        msg.msg_iovlen = length;
        return ::sendmsg( m_sock, &msg, flags );
    }
    return -1;
}

int MSocket::sendTo( const void* buffer, size_t length, const Address::ptr to, int flags )
{
    if ( isConnected() )
    {
        return ::sendto( m_sock, buffer, length, flags, to->getAddr(), to->getAddrLen() );
    }
    return -1;
}

int MSocket::sendTo( const iovec* buffers, size_t length, const Address::ptr to, int flags )
{
    if ( isConnected() )
    {
        msghdr msg;
        memset( &msg, 0, sizeof( msg ) );
        msg.msg_iov     = ( iovec* )buffers;
        msg.msg_iovlen  = length;
        msg.msg_name    = to->getAddr();
        msg.msg_namelen = to->getAddrLen();
        return ::sendmsg( m_sock, &msg, flags );
    }
    return -1;
}

int MSocket::recv( void* buffer, size_t length, int flags )
{
    if ( isConnected() )
    {
        if ( ::recv( m_sock, buffer, length, flags ) == -1 )
        {
            ERROR_STD_STREAM_LOG( g_logger ) << "receive error errno=" << S( errno )
                                             << " errstr=" << strerror( errno ) << Logger::endl();
            return -1;
        }
        return 1;
    }
    return -1;
}

int MSocket::recv( iovec* buffers, size_t length, int flags )
{
    if ( isConnected() )
    {
        msghdr msg;
        memset( &msg, 0, sizeof( msg ) );
        msg.msg_iov    = ( iovec* )buffers;
        msg.msg_iovlen = length;
        return ::recvmsg( m_sock, &msg, flags );
    }
    return -1;
}

int MSocket::recvFrom( void* buffer, size_t length, Address::ptr from, int flags )
{
    if ( isConnected() )
    {
        socklen_t len = from->getAddrLen();
        return ::recvfrom( m_sock, buffer, length, flags, from->getAddr(), &len );
    }
    return -1;
}

int MSocket::recvFrom( iovec* buffers, size_t length, Address::ptr from, int flags )
{
    if ( isConnected() )
    {
        msghdr msg;
        memset( &msg, 0, sizeof( msg ) );
        msg.msg_iov     = ( iovec* )buffers;
        msg.msg_iovlen  = length;
        msg.msg_name    = from->getAddr();
        msg.msg_namelen = from->getAddrLen();
        return ::recvmsg( m_sock, &msg, flags );
    }
    return -1;
}

Address::ptr MSocket::getRemoteAddress()
{
    if ( m_remoteAddress )
    {
        return m_remoteAddress;
    }

    Address::ptr result;
    switch ( m_family )
    {
        case AF_INET:
            result.reset( new IPv4Address() );
            break;
        case AF_INET6:
            result.reset( new IPv6Address() );
            break;
        case AF_UNIX:
            result.reset( new UnixAddress() );
            break;
        default:
            result.reset( new UnknownAddress( m_family ) );
            break;
    }
    socklen_t addrlen = result->getAddrLen();
    if ( getpeername( m_sock, result->getAddr(), &addrlen ) )
    {
        // SYLAR_LOG_ERROR(g_logger) << "getpeername error sock=" << m_sock
        //    << " errno=" << errno << " errstr=" << strerror(errno);
        return Address::ptr( new UnknownAddress( m_family ) );
    }
    if ( m_family == AF_UNIX )
    {
        UnixAddress::ptr addr = std::dynamic_pointer_cast< UnixAddress >( result );
        addr->setAddrLen( addrlen );
    }

    m_remoteAddress = result;
    return m_remoteAddress;
}

Address::ptr MSocket::getLocalAddress()
{
    if ( m_localAddress )
    {
        return m_localAddress;
    }

    Address::ptr result;
    switch ( m_family )
    {
        case AF_INET:
            result.reset( new IPv4Address() );
            break;
        case AF_INET6:
            result.reset( new IPv6Address() );
            break;
        case AF_UNIX:
            result.reset( new UnixAddress() );
            break;
        default:
            result.reset( new UnknownAddress( m_family ) );
            break;
    }
    socklen_t addrlen = result->getAddrLen();
    if ( getsockname( m_sock, result->getAddr(), &addrlen ) )
    {
        ERROR_STD_STREAM_LOG( g_logger )
        << "getsockname error sock=" << S( m_sock ) << " errno=" << S( errno )
        << " errstr=" << strerror( errno ) << Logger::endl();
        return Address::ptr( new UnknownAddress( m_family ) );
    }
    if ( m_family == AF_UNIX )
    {
        UnixAddress::ptr addr = std::dynamic_pointer_cast< UnixAddress >( result );
        addr->setAddrLen( addrlen );
    }
    m_localAddress = result;
    return m_localAddress;
}

bool MSocket::isValid() const { return m_sock != -1; }

int MSocket::getError()
{
    int error     = 0;
    socklen_t len = sizeof( error );
    if ( !getOption( SOL_SOCKET, SO_ERROR, &error, &len ) )
    {
        error = errno;
    }
    return error;
}

std::ostream& MSocket::dump( std::ostream& os ) const
{
    os << "[MSocket sock=" << m_sock << " is_connected=" << m_isConnected
       << " family=" << m_family << " type=" << m_type << " protocol=" << m_protocol;
    if ( m_localAddress )
    {
        os << " local_address=" << m_localAddress->toString();
    }
    if ( m_remoteAddress )
    {
        os << " remote_address=" << m_remoteAddress->toString();
    }
    os << "]";
    return os;
}

std::string MSocket::toString() const
{
    std::stringstream ss;
    dump( ss );
    return ss.str();
}

void MSocket::initSock()
{
    int val = 1;
    setOption( SOL_SOCKET, SO_REUSEADDR, val );
    if ( m_type == SOCK_STREAM )
    {
        setOption( IPPROTO_TCP, TCP_NODELAY, val );
    }
}

void MSocket::newSock()
{
    m_sock = socket( m_family, m_type, m_protocol );
    if ( STAR_UNLIKELY( m_sock != -1 ) )
    {
        initSock();
    }
    else
    {
        ERROR_STD_STREAM_LOG( g_logger )
        << "socket(" << S( m_family ) << ", " << S( m_type ) << ", " << S( m_protocol )
        << ") errno=" << S( errno ) << " errstr=" << strerror( errno ) << Logger::endl();
    }
}
}
