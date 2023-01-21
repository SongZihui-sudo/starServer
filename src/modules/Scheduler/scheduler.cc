#include "./scheduler.h"
#include "modules/common/common.h"
#include "modules/log/log.h"

#include <aco.h>
#include <string>
#include <unistd.h>
#include <vector>

namespace star
{
std::vector< void* > Schedule_args = {};
pthread_mutex_t mutex;
int Schedule_answer = -1;

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
        FATAL_STD_STREAM_LOG( this->m_logger )
        << "The number of threads has reached the maximum, and new tasks cannot be "
           "assigned."
        << "%n%0";
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
            DOTKNOW_STD_STREAM_LOG( this->m_logger ) << "NOT FOUND THE TASK!%n%0";
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
            DOTKNOW_STD_STREAM_LOG( this->m_logger ) << "NOT FOUND THE TASK!%n%0";
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

void Scheduler::start()
{
    Scheduler* Self = ( Scheduler* )self;

    INFO_STD_STREAM_LOG( Self->m_logger ) << std::to_string( getTime() ) << " <----> "
                                          << "Scheduler Staring!"
                                          << "%n%0";

    while ( true )
    {
        if ( !Schedule_args.empty() )
        {
            std::string temp = "";
            std::function< void() > temp1;
            switch ( *( int* )Schedule_args[0] )
            {
                case 1:
                    INFO_STD_STREAM_LOG( Self->m_logger )
                    << std::to_string( getTime() ) << " <----> "
                    << "Scheduler flag: " << std::to_string( 1 ) << "%n%0";
                    /* 注册任务 */
                    Self->Regist_task( *( std::function< void() >* )Schedule_args[1],
                                       ( ( std::string* )Schedule_args[2] )->c_str() );
                    Schedule_args.clear();
                    Schedule_answer = 1;
                    break;
                case 2:
                    INFO_STD_STREAM_LOG( Self->m_logger )
                    << std::to_string( getTime() ) << " <----> "
                    << "Scheduler flag: " << std::to_string( 2 ) << "%n%0";
                    Self->delete_task( ( ( std::string* )Schedule_args[1] )->c_str() );
                    Schedule_args.clear();
                    Schedule_answer = 1;
                    break;
                case 3:
                    INFO_STD_STREAM_LOG( Self->m_logger )
                    << std::to_string( getTime() ) << " <----> "
                    << "Scheduler flag: " << std::to_string( 3 ) << "%n%0";
                    /* 重设任务 */
                    Self->reset_task( ( ( std::string* )Schedule_args[2] )->c_str(),
                                      *( std::function< void() >* )Schedule_args[1] );
                    Schedule_args.clear();
                    Schedule_answer = 1;
                    break;
                case 4:
                    INFO_STD_STREAM_LOG( Self->m_logger )
                    << std::to_string( getTime() ) << " <----> "
                    << "Scheduler flag: " << std::to_string( 4 ) << "%n%0";
                    /* 分配任务 */
                    for ( size_t i = 0; i < Self->m_tasks.size(); i++ )
                    {
                        if ( Self->m_tasks[i].t_status == INIT )
                        {
                            Self->assign_task( Self->m_tasks[i] );
                        }
                    }
                    Schedule_args.clear();
                    Schedule_answer = 1;
                    break;
                case 5:
                    INFO_STD_STREAM_LOG( Self->m_logger )
                    << std::to_string( getTime() ) << " <----> "
                    << "Scheduler flag: " << std::to_string( 5 ) << "%n%0";

                    WERN_STD_STREAM_LOG( Self->m_logger ) << "The scheduler process ends."
                                                          << "%n%0";
                    /* 线程结束 */
                    Schedule_answer = 2;
                    return;
                default:
                    WERN_STD_STREAM_LOG( Self->m_logger ) << "Unknown scheduler signal！"
                                                          << "%n%0";
                    Schedule_args.clear();
                    break;
            }
        }
        else
        {
            /* 休眠一下 */
            sleep( 1 );
        }
    }
    return;
}

void Scheduler::run()
{
    /* 新建线程来执行start */
    INFO_STD_STREAM_LOG( this->m_logger ) << std::to_string( getTime() ) << " <----> "
                                           << "Scheduler Begin Runing!"
                                           << "%n%0";
    self                             = this; /* 传递this指针 */
    std::function< void() > func_ptr = this->start;
    m_thread.reset( new Threading( func_ptr, "Scheduler" ) );
    m_id = m_thread->get_id();
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
    for ( size_t i = 0; i < this->m_tasks.size(); i++ )
    {
        if ( this->m_tasks[i].t_status == INIT )
        {
            this->assign_task( this->m_tasks[i] );
        }
    }
}

}
