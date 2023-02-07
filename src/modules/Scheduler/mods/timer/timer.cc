#include "./timer.h"
#include "../../../log/log.h"
#include "modules/common/common.h"
#include "modules/thread/thread.h"

#include <cstdint>
#include <functional>
#include <pthread.h>
#include <type_traits>

namespace star
{

/* 定时器的运行线程 */
thread_local int64_t Timer::start_time = 0;
static Logger::ptr g_logger( STAR_NAME( "TIMER_LOGGER" ) );
static int32_t timer_counter = 1;
int32_t Timer::m_secends = 0;
std::function< void() > Timer::callback_func = nullptr;

Timer::Timer( std::function< void() > cb, int32_t senc )
{
    this->m_secends = senc; /* 定时时间 */
    this->callback_func   = cb;
}

void Timer::run()
{
    /* 新建一个线程来运行定时器 */
    this->Timer_thread.reset( new Threading( std::function< void() >( this->start ),
                                       "Timer_thread" + S( timer_counter ) ) );
    timer_counter++;
}

void Timer::start()
{
    /* 获取开始时间 */
    Timer::start_time = getTime();
    INFO_STD_STREAM_LOG( g_logger ) << "%D"
                                    << "Timer Start!"
                                    << Logger::endl();
    /* 阻塞进程，等待到达时间后执行回调函数 */
    while ( true )
    {
        int64_t current_time = getTime();
        // DEBUG_STD_STREAM_LOG( g_logger ) << "%D"
        //                                  << Logger::endl();
        /* 到达时间 */
        int64_t last_time = current_time - start_time;
        if ( last_time > Timer::m_secends )
        {
            INFO_STD_STREAM_LOG( g_logger ) << "%D"
                                            << "Timer End! Call call-back function!" << S(last_time) << "|" << S(Timer::m_secends)
                                            << Logger::endl();
            /* 执行回调函数 */
            Timer::callback_func();
            break; /* 退出循环 */
        }
    }
}

void Timer::interrupt()
{
    /* 终止进程 */
    Timer_thread->exit();
    this->m_stop_time = getTime();
}

void Timer::stop()
{
    /* 终止进程 */
    Timer_thread->exit();
    this->m_stop_time = getTime();
    /* 执行回调函数 */
    callback_func();
}

}