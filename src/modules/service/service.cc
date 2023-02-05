#include "service.h"
#include "modules/log/log.h"
#include "modules/thread/thread.h"
#include <functional>
#include <pthread.h>
#include <tuple>
#include <unistd.h>
#include <utility>

namespace star
{

static Logger::ptr g_logger( STAR_NAME( "servers_manager_logger" ) );

void service_manager::init( std::vector< std::tuple< std::string, std::function< void() > > > services )
{
    for ( auto item : services )
    {
        /* 注册任务 */
        this->register_service( std::get< 0 >( item ), std::get< 1 >( item ) );
    }
}

void service_manager::start()
{
    /* 依次启动 */
    for ( auto item : this->service_pool )
    {
        if ( item.second.status != Runing )
        {
            Threading::ptr service_thread(
            new Threading( this->service_pool[item.first].func, item.first ) );
            INFO_STD_STREAM_LOG( g_logger )
            << "%D"
            << "Service: " << item.first << " Start!"
            << "thread: " << S( service_thread->get_id() ) << Logger::endl();
            item.second.thread = service_thread; /* 设置id */
            item.second.status = Runing;
        }

        sleep( 1 ); /* 暂停一下，*****不能删***** */
    }
}

void service_manager::stop( std::string name )
{
    if ( !this->current_services_num )
    {
        FATAL_STD_STREAM_LOG( g_logger )
        << "%D"
        << "No service_manager has been register!" << Logger::endl();
        return;
    }
    pthread_t id = this->service_pool[name].thread->get_id();
    this->service_pool[name].status = Free;
    /* 结束进程 */
    pthread_detach( id );
    this->current_services_num--;
    WERN_STD_STREAM_LOG( g_logger ) << "%D"
                                    << "Service: " << name << " End!" << Logger::endl();
}

void service_manager::register_service( std::string name, std::function< void() > func )
{
    if ( this->current_services_num > this->max_services_num - 1 )
    {
        FATAL_STD_STREAM_LOG( g_logger )
        << "The maximum number of tasks has been reached." << Logger::endl();
        return;
    }

    service new_service = { .name = name, .func = func, .thread = nullptr, .status = Init };
    this->service_pool.emplace( name, new_service );

    INFO_STD_STREAM_LOG( g_logger ) << "%D"
                                    << "Service: " << name << " Register!" << Logger::endl();
    this->current_services_num++;
}

}