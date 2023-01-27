#include <chrono>
#include <ctime>
#include <iomanip>
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "./common.h"

pid_t GetThreadId() { return static_cast< pid_t >( ::syscall( SYS_gettid ) ); }

std::uint64_t getTime()
{
    auto now   = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t( now );
    std::ostringstream ss;
    ss << std::put_time( localtime( &now_c ), "%Y%m%d%H%M%S" );
    std::string timeid = ss.str();
    std::uint64_t ret = strtoll( timeid.c_str(), nullptr, 0 ); /* 获取到的时间为世界时相比于北京时间小八个小时 */
    return ret;
}

int get_length( long x )
{
    int leng = 1;
    while ( x > 10 )
    {
        x /= 10;
        leng++;
    }
    return leng;
}

std::string random_string( size_t len )
{
    int real_len = rand() % len;
    if ( real_len == 0 )
        return "";
    char* str = ( char* )malloc( real_len );

    for ( int i = 0; i < real_len; ++i )
    {
        switch ( ( rand() % 3 ) )
        {
            case 1:
                str[i] = 'A' + rand() % 26;
                break;
            case 2:
                str[i] = 'a' + rand() % 26;
                break;
            default:
                str[i] = '0' + rand() % 10;
                break;
        }
    }
    str[real_len - 1]  = '\0';
    std::string string = str;
    free( str );
    return string;
}