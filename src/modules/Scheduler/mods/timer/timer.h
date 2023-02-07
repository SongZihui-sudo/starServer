#ifndef TIMER_H
#define TIMER_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "../../../thread/thread.h"

namespace star
{
/*
    定时器类
    给一个指定的线程计时
 */
class Timer
{
public:
    typedef std::shared_ptr< Timer > ptr;

    Timer( std::function< void() > cb, int32_t senc );

    ~Timer() = default;

public:
    /*
        运行定时器
    */
    void run();

    /*
        开始计时
    */
    static void start();

    /*
        打断计时
    */
    void interrupt();

    /*
        提前结束计时
    */
    void stop();

    /*
        获取停止时间
    */
    int64_t get_stop_time() { return this->m_stop_time; }

    /*
        获取状态
     */
    Threading::Thread_Status get_status() { return this->Timer_thread->get_status(); }

private:
    static thread_local int64_t start_time;
    Threading::ptr Timer_thread;
    static int32_t m_secends; /* 定时时间 */
    int64_t m_stop_time;      /* 停止时间 */
    static std::function< void() > callback_func;
};
}

#endif