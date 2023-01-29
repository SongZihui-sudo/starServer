#ifndef LEASE_H
#define LEASE_H

#include "aco.h"
#include "modules/Scheduler/mods/timer/timer.h"

#include <cstdint>
#include <inttypes.h>
#include <memory>
#include <string>

namespace star
{
/* 租约，颁给操作的执行者，在一定时间内有效，超时时间无限，颁发者不认执行者执行的动作 */
class lease
{
public:
    typedef std::shared_ptr< lease > ptr;
    lease( int32_t lease_times );
    ~lease() = default;

public:
    /* 续约 */
    void renew();

    /* 租约是否有效 */
    bool is_available() { return available; }

    /* 租约失效的回调函数 */
    static void lease_invalid();

    /* 获取id */
    std::string get_id() { return this->m_id; }

private:
    bool available     = false;
    std::string m_id   = "";
    int32_t m_secends  = 0;       /* 租约时长 */
    Timer::ptr m_timer = nullptr; /* 定时器 */
};

/*
    租约管理器
 */
class lease_manager
{
public:
    typedef std::shared_ptr< lease_manager > ptr;

    lease_manager( int32_t default_lease_time ){};
    ~lease_manager() = default;

public:
    /* 授权新租约 */
    void new_lease();

    /* 销毁租约 */
    void destory_lease( std::string id );

    /* 续约 */
    void renew_lease( std::string id );

    /* 检查租约是不是全部过期 */
    bool is_all_late();

    /* 检查全部租约，然后把过期的租约销毁掉 */
    void destory_invalid_lease();

private:
    std::map< std::string, lease::ptr > lease_tab; /* 客户端与租约的表 */
    int32_t default_lease_time;
};

}

#endif