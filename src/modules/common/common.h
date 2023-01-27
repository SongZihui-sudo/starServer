#ifndef COMMON_H
#define COMMON_H

#include <thread>
#include <byteswap.h>

/*
    获取线程 id
*/
pid_t GetThreadId();

/*
    获取时间
*/
std::uint64_t getTime();

int get_length( long x );

/**
 * @brief 8字节类型的字节序转化
 */
template< class T >
typename std::enable_if< sizeof( T ) == sizeof( uint64_t ), T >::type byteswap( T value )
{
    return ( T )bswap_64( ( uint64_t )value );
}

/**
 * @brief 4字节类型的字节序转化
 */
template< class T >
typename std::enable_if< sizeof( T ) == sizeof( uint32_t ), T >::type byteswap( T value )
{
    return ( T )bswap_32( ( uint32_t )value );
}

/**
 * @brief 2字节类型的字节序转化
 */
template< class T >
typename std::enable_if< sizeof( T ) == sizeof( uint16_t ), T >::type byteswap( T value )
{
    return ( T )bswap_16( ( uint16_t )value );
}

/**
 * @brief 只在小端机器上执行byteswap, 在大端机器上什么都不做
 */
template< class T >
T byteswapOnLittleEndian( T t )
{
    return t;
}

/**
 * @brief 只在大端机器上执行byteswap, 在小端机器上什么都不做
 */
template< class T >
T byteswapOnBigEndian( T t )
{
    return byteswap( t );
}

/* 生成随机字符串 */
std::string random_string( size_t len );

#endif