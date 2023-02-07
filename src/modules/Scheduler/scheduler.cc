#include "./scheduler.h"
#include "modules/common/common.h"
#include "modules/log/log.h"

#include <aco.h>
#include <cstdint>
#include <functional>
#include <pthread.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace star
{
std::vector< void* > Schedule_args = {};
pthread_mutex_t mutex;
int Schedule_answer                                = -1;
int64_t Scheduler::thread_free_time                = 0;
std::vector< Threading::ptr > Scheduler::m_threads = {}; /* 线程池 */
static Logger::ptr g_logger( STAR_NAME( "scheduler_logger" ) );

Scheduler::Scheduler( size_t max_threads, int64_t thread_free_time )
{
    this->max_threads      = max_threads;
    this->thread_free_time = thread_free_time;
}

void Scheduler::Regist_task( std::function< void() > t_func, std::string t_name )
{
    /* 加入新任务 */
    this->m_tasks.push_back( task( t_func, t_name ) );
}

void Scheduler::assign_task( task& t_task )
{
    /* 查看进程池中是否有空闲进程 */
    for ( size_t i = 0; i < this->m_threads.size(); i++ )
    {
        if ( !this->m_threads[i]->get_status() )
        {
            /* 为这个线程分配任务函数 */
            t_task.t_status = RUNING;
            this->m_threads[i]->reset( t_task.t_func, t_task.t_name );
            return;
        }
    }

    /* 如果没有空闲进程，并且线程数小于最大进程数就新建一个进程 */
    if ( this->m_threads.size() < this->max_threads )
    {
        /* 新建一个线程，把这个任务函数分配给它，并加入线程池 */
        t_task.t_status = RUNING;
        Threading::ptr new_thread( new Threading( t_task.t_func, t_task.t_name ) );
        this->m_threads.push_back( new_thread );
    }
    else
    {
        /* 无法创建新线程，达到上限，分配任务失败 */
        FATAL_STD_STREAM_LOG( g_logger )
        << "The number of threads has reached the maximum, and new tasks cannot be "
           "assigned."
        << Logger::endl();
    }
}

void Scheduler::delete_task( std::string t_name )
{
    /* 查找任务 */
    arr       = this->m_tasks; /* 更新arr */
    find_name = t_name;
    aco_thread_init( NULL );                                  /* 初始化协程 */
    aco_t* main_co = aco_create( NULL, NULL, 0, NULL, NULL ); /* 主协程 */

    aco_share_stack_t* sstk1 = aco_share_stack_new( 0 ); /* 运行栈 */
    find_begin               = 0;
    find_end                 = int( arr.size() / 2 );
    aco_t* co1 = aco_create( main_co, sstk1, 0, this->find_value, nullptr ); /* 创建协程 1 */

    aco_share_stack_t* sstk2 = aco_share_stack_new( 0 ); /* 运行栈 */
    find_begin               = int( arr.size() / 2 );
    find_end                 = arr.size();
    aco_t* co2 = aco_create( main_co, sstk2, 0, this->find_value, nullptr ); /* 创建协程2 */

    aco_resume( co1 );
    aco_resume( co2 );

    /* 等待拿到结果，重新设置任务 */
    while ( result_index != -1 )
    {
        if ( result_index == -2 )
        {
            DOTKNOW_STD_STREAM_LOG( g_logger ) << "NOT FOUND THE TASK!%n%0";
            break;
        }
        else if ( result_index > 0 )
        {
            /* 删除元素 */
            this->m_tasks.erase( this->m_tasks.begin() + result_index );
            break;
        }
        else
        {
            continue;
        }
    }

    aco_share_stack_destroy( sstk1 ); /* 销毁运行栈 */
    aco_share_stack_destroy( sstk2 ); /* 销毁运行栈 */
    aco_destroy( co1 );               /* 销毁协程1 */
    aco_destroy( co2 );               /* 销毁协程2 */
}

void Scheduler::reset_task( std::string t_name, std::function< void() > t_func )
{
    /* 查找任务 */
    arr       = this->m_tasks; /* 更新arr */
    find_name = t_name;
    aco_thread_init( NULL );                                  /* 初始化协程 */
    aco_t* main_co = aco_create( NULL, NULL, 0, NULL, NULL ); /* 主协程 */

    aco_share_stack_t* sstk1 = aco_share_stack_new( 0 ); /* 运行栈 */
    find_begin               = 0;
    find_end                 = int( arr.size() / 2 );
    aco_t* co1 = aco_create( main_co, sstk1, 0, this->find_value, nullptr ); /* 创建协程 1 */

    aco_share_stack_t* sstk2 = aco_share_stack_new( 0 ); /* 运行栈 */
    find_begin               = int( arr.size() / 2 );
    find_end                 = arr.size();
    aco_t* co2 = aco_create( main_co, sstk2, 0, this->find_value, nullptr ); /* 创建协程2 */

    aco_resume( co1 );
    aco_resume( co2 );

    /* 等待拿到结果，重新设置任务 */
    while ( result_index != -1 )
    {
        if ( result_index == -2 )
        {
            DOTKNOW_STD_STREAM_LOG( g_logger ) << "NOT FOUND THE TASK!%n%0";
            break;
        }
        else if ( result_index > 0 )
        {
            this->m_tasks[result_index].t_func   = t_func; /* 重新设置任务函数 */
            this->m_tasks[result_index].t_status = INIT;
            break;
        }
        else
        {
            continue;
        }
    }

    aco_share_stack_destroy( sstk1 ); /* 销毁运行栈 */
    aco_share_stack_destroy( sstk2 ); /* 销毁运行栈 */
    aco_destroy( co1 );               /* 销毁协程1 */
    aco_destroy( co2 );               /* 销毁协程2 */
}

void Scheduler::find_value()
{
    for ( size_t i = find_begin; i < find_end; i++ )
    {
        if ( arr[i] == find_name )
        {
            result_index = i;
            aco_yield(); /* 协程退出 */
            break;
        }
    }

    if ( result_index < 0 )
    {
        result_index = -2;
    }
}

void Scheduler::manage()
{
    /* 调度还没运行的任务 */
    while ( !this->m_tasks.empty() )
    {
        this->assign_task( this->m_tasks.front() );
        this->m_tasks.pop_front();
    }
}

void Scheduler::check_free_thread()
{
    INFO_STD_STREAM_LOG( g_logger ) << "Begin check the free thread in pool" << Logger::endl();
    if ( Scheduler::m_threads.empty() )
    {
        INFO_STD_STREAM_LOG( g_logger ) << "NO Thread in the Pool" << Logger::endl();
        sleep( Scheduler::thread_free_time );
        INFO_STD_STREAM_LOG( g_logger ) << "End check the free thread in pool" << Logger::endl();
        check_free_thread();
    }
    /* 查看空闲的进程, 空闲时间过长的话，杀死这个进程*/
    int64_t current_time = getTime();
    int i                = 0;
    for ( auto item : Scheduler::m_threads )
    {
        if ( item->get_status() == Threading::FREE )
        {
            int64_t free_time = current_time - item->get_task_end_time();
            if ( free_time > Scheduler::thread_free_time )
            {
                pid_t thread_id         = item->get_id();
                std::string thread_name = item->get_name();
                Scheduler::m_threads.erase( Scheduler::m_threads.begin() + i );
                PRINT_LOG( g_logger,
                           "%p Thread: %N id: %t thread free time: %d Remove "
                           "Succesfully!%n",
                           "DEBUG",
                           thread_name.c_str(),
                           thread_id,
                           free_time );
            }
        }
        i++;
    }
    INFO_STD_STREAM_LOG( g_logger ) << "End check the free thread in pool" << Logger::endl();
    sleep( Scheduler::thread_free_time );
    check_free_thread();
}

}
