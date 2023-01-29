#ifndef SERVICE_H
#define SERVICE_H

#include "modules/Scheduler/scheduler.h"
#include "modules/thread/thread.h"
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <pthread.h>
#include <string>
#include <tuple>
#include <vector>

namespace star
{
/*
    服务器的服务任务（需要长期运行的一些任务）,这些任务，开机自启
 */
class service_manager
{
protected:
    struct service
    {
        std::string name;
        std::function< void() > func;
        Threading::ptr thread;
    };

public:
    typedef std::shared_ptr< service_manager > ptr;

    service_manager( size_t max_services_num )
    {
        this->max_services_num     = max_services_num;
        this->current_services_num = 0;
        this->service_pool         = {};
    }

    ~service_manager() = default;

public:
    /*
        初始化
     */
    void init( std::vector< std::tuple< std::string, std::function< void() > > > services );

    /*
        启动注册的服务
    */
    void start();

    /*
        停止指定的服务
     */
    void stop( std::string name );

    /*
        注册新的服务
     */
    void register_service( std::string name, std::function< void() > service_manager );

    /* 当前注册的服务的数量 */
    size_t service_num() { return this->service_pool.size(); }

private:
    size_t current_services_num;
    size_t max_services_num;
    std::map< std::string, service > service_pool;
};
}

#endif