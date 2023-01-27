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

    Timer( std::function< void( ) > cb, int32_t senc );

    ~Timer() = default;

public:
    /* 运行定时器 */
    void run();

    /* 开始计时 */
    static void start();

private:
    int32_t m_secends;                   /* 定时时间 */
};
}

#endif