#ifndef LEASE_H
#define LEASE_H

#include "aco.h"
#include "modules/Scheduler/mods/timer/timer.h"
#include "modules/thread/thread.h"

#include <cstdint>
#include <inttypes.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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
    /*
        续约
    */
    void renew();

    /*
        租约是否有效
    */
    bool is_available() { return available; }

    /*
        租约失效的回调函数
    */
    static void lease_invalid();

    /*
        获取id
    */
    std::string get_id() { return this->m_id; }

    /*
        获取定时器线程状态
     */
    Threading::Thread_Status get_status() { return m_timer->get_status(); }

    void start();

private:
    static bool available;
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

    lease_manager( int32_t default_lease_time )
    {
        this->default_lease_time = default_lease_time;
    };
    ~lease_manager() = default;

public:
    /* 授权新租约 */
    void new_lease( std::string file_id, std::string client_id );

    /* 销毁租约 */
    void destory_lease( std::string lease_id );

    /* 续约 */
    void renew_lease( std::string id );

    /* 检查租约是不是全部过期 */
    bool is_all_late();

    /* 检查全部租约，然后把过期的租约销毁掉 */
    void destory_invalid_lease();

    /* 通过 id 查询租约是否过期 */
    bool is_available_by_id( std::string lease_id )
    {

        return this->lease_tab[lease_id]->is_available();
    }

    /* 通过客户端查询租约是否过期 */
    bool is_available_by_client( std::string client_id )
    {

        return this->m_client_to_lease[client_id]->is_available();
    }

    /* 通过文件查询租约是否过期 */
    bool is_availeable_by_file( std::string file_id )
    {
        return this->lease_tab[this->m_file_to_lease[file_id]]->is_available();
    }

    /* 通过文件 id 获取 lease id */
    std::string get_leaseid_by_file( std::string file_id )
    {

        return this->m_file_to_lease[file_id];
    }

    /* 通过client id 获取 lease id */
    std::string get_leaseid_by_client( std::string client_id )
    {

        return this->m_client_to_lease[client_id]->get_id();
    }

    /* 通过lease id 获取 lease 对象 */
    lease::ptr get_lease( std::string lease_id ) { return this->lease_tab[lease_id]; }

    /* 通过 lease id 获取 client id */
    std::string get_client_id( std::string lease_id )
    {
        return this->lease_to_client[lease_id];
    }

private:
    std::map< std::string, lease::ptr > lease_tab; /* 客户端与租约的表 */
    std::unordered_map< std::string, std::string > lease_to_client; /* 通过 lease id 获取 client id */
    int32_t default_lease_time;
    std::unordered_map< std::string, std::vector< std::string > > m_lease_id__to_file; /* 通过租约id索引文件 */
    std::unordered_map< std::string, lease::ptr > m_client_to_lease; /* 客户端与租约的索引 */
    std::unordered_map< std::string, std::string > m_file_to_lease; /* file 与 租约 id 的索引 */
};

}

#endif