#ifndef LEASE_H
#define LEASE_H

#include "modules/Scheduler/mods/timer/timer.h"
#include <inttypes.h>

namespace star
{
/* 租约，颁给操作的执行者，在一定时间内有效，超时时间无限，颁发者不认执行者执行的动作 */
class lease
{
public:
    lease();
    ~lease();

public:
    /* 把租约给一个指定的线程 */
    void give();

    /* 续约 */
    void renew();

    /* 租约是否有效 */
    void is_available();

private:
    int32_t m_secends;  /* 租约时长 */
    Timer::ptr m_timer; /* 定时器 */
};

class lease_manager
{
public:
    /* 检查租约 */
    

private:
    std::map<pid_t, lease> lease_tab;   /* 线程与租约的表 */
};

}

#endif