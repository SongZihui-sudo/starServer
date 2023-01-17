/*
 * author: SongZihui-sudo
 * file: thead.h
 * desc: 线程模块
 * date: 2023-1-4
 */

#ifndef THREAD_H
#define THREAD_H

#include <functional>
#include <memory>
#include <semaphore.h>
#include <signal.h>
#include <string>
#include <thread>

#include "../log/log.h"

namespace star
{
/*
    线程类
*/
class Threading
{
protected:
    enum Thread_Status
    {
        FREE   = 0,
        RUNING = 1,
        INIT   = 2,
        KILLED = 3,
        ERROR  = 4
    };

public:
    typedef std::shared_ptr< Threading > ptr;

    Threading( std::function< void() > func, const std::string& name = "UNKNOW");

    ~Threading()
    {
        if ( m_thread )
        {
            pthread_detach( m_thread );
        }
    }

public:
    /*
        重设
     */
    void reset( std::function< void() > func );

    /*
        阻塞 等待 threadid 线程运行结束之后再运行
    */
    void join();
    /*
        结束线程
    */
    void exit();

    /* 获取信号量 */
    sem_t get_sem() { return this->m_sem; }

    /* 获取执行函数 */
    std::function< void() > get_func() { return this->func; }

    /* 设置id */
    void set_id( pid_t in_id ) { this->m_id = in_id; }

    /* 获取id */
    pid_t get_id() const { return this->m_id; }

    /* 获取线程名 */
    std::string get_name() const { return this->m_name; }

    /* 结束一个进程 */
    void cancel( pid_t t_id ) { pthread_cancel( t_id ); }

    /* 获取线程状态 */
    Thread_Status get_status() { return this->m_status; }

private:
    /*
        运行线程
    */
    static void* run( void* arg );

private:
    /* 删除一些函数 */
    Threading( Threading& ) = delete;

    Threading( Threading&& ) = delete;

    void operator=( Threading& ) = delete;

private:
    Thread_Status m_status;              /* 线程状态 */
    pid_t m_id;                   /* 线程id */
    std::string m_name;           /* 线程名 */
    pthread_t m_thread = 0;       /* 线程 */
    std::function< void() > func; /* 线程执行函数 */
    sem_t m_sem;                  /* 信号量 */
    star::Logger::ptr t_logger;   /* 日志器 */
};
}

#endif