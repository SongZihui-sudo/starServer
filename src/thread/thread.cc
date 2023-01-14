/*
 * author: SongZihui-sudo
 * file: thead.cc
 * desc: 线程模块
 * date: 2023-1-4
 */

#include "./thread.h"
#include "../common/common.h"
#include "../log/log.h"
#include "./fiber.h"

#include <functional>

namespace star
{

static thread_local Threading* cur_thread = nullptr; /* 当前线程 */

static thread_local std::string cur_thread_name = "UNKNOW"; /* 当前线程名 */

static Logger::ptr thread_logger( new Logger ); /* 日志器 */

Threading::Threading( std::function< void() > func, const std::string& name )
{
    sem_init( &this->m_sem, 0, 1 );                /* 初始化信号量 */
    thread_logger->set_name( "Threading-Logger" ); /* 设置日志器名 */
    this->m_name = name;                           /* 线程名 */
    this->func   = func;                           /* 线程中运行的函数 */
    t_fiber.reset(new fiber());
    int flag = pthread_create( &this->m_thread, nullptr, &Threading::run, this ); /* 创建线程 */

    if ( !flag ) /* 判断是否线程创建成功 */
    {
        INFO_STD_STREAM_LOG( thread_logger ) << "Create Threading is successfully!%n"
                                             << "%0";
        return;
    }
    sem_wait( &this->m_sem );
}

void Threading::join()
{
    if ( m_thread )
    {
        /* 调用 pthead的json函数，等待 */
        int rt = pthread_join( m_thread, nullptr );
        if ( rt )
        {
            FATAL_STD_STREAM_LOG( thread_logger ) << "Threading Join FAIL!%n"
                                                  << "%0";
            throw std::logic_error( "pthread_join error" );
        }

        m_thread = 0;
    }
}

Threading* Threading::GetThis()
{
    /* 判断是否存在线程 */
    return cur_thread;
}

const std::string& Threading::GetName() { return cur_thread_name; }

void Threading::SetName( const std::string& name )
{
    if ( cur_thread )
    {
        cur_thread->m_name = name;
        return;
    }
    cur_thread_name = name;
}

void* Threading::run( void* arg )
{
    Threading* thread = ( Threading* )arg;
    cur_thread        = thread;
    cur_thread_name   = thread->m_name;
    thread->m_id      = GetThreadId();
    pthread_setname_np( pthread_self(), thread->m_name.substr( 0, 15 ).c_str() );

    std::function< void() > cb;
    cb.swap( thread->func );

    cb();
    sem_post( &thread->m_sem );
    return 0;
}

}