#include "./timer.h"
#include "../../../log/log.h"
#include "modules/thread/thread.h"

#include <cstdint>
#include <functional>
#include <type_traits>

namespace star
{

/* 定时器的运行线程 */
static Threading::ptr Timer_thread;
static thread_local int64_t start_time;
static int32_t secends;
static Logger::ptr g_logger( STAR_NAME( "TIMER_LOGGER" ) );
static int32_t timer_counter = 1;
static std::function< void() > callback_func;

Timer::Timer( std::function< void() > cb, int32_t senc )
{
    this->m_secends = senc; /* 定时时间 */
    secends         = senc;
    callback_func   = cb;
}

void Timer::run()
{
    /* 新建一个线程来运行定时器 */
    Timer_thread.reset( new Threading( std::function< void() >( this->start ),
                                       "Timer_thread" + S( timer_counter ) ) );
    timer_counter++;
}

void Timer::start()
{
    /* 获取开始时间 */
    start_time = getTime();
    INFO_STD_STREAM_LOG( g_logger ) << "%D"
                                    << "Timer Start!"
                                    << "%n%0";
    /* 阻塞进程，等待到达时间后执行回调函数 */
    while ( true )
    {
        int64_t current_time = getTime();
        //DEBUG_STD_STREAM_LOG( g_logger ) << "%D"
        //                                 << "%n%0";
        /* 到达时间 */
        if ( current_time - start_time == secends )
        {
            INFO_STD_STREAM_LOG( g_logger ) << "%D"
                                            << "Timer End! Call call-back function!"
                                            << "%n%0";
            /* 执行回调函数 */
            callback_func();
            break; /* 退出循环 */
        }
    }
}

}