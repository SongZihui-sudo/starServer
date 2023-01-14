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
public:
    typedef std::shared_ptr< Threading > ptr;
    Threading( std::function< void() > func, const std::string& name = "UNKNOW" );
    ~Threading()
    {
        if ( m_thread )
        {
            pthread_detach( m_thread ); /* 销毁线程 */
        }
    };

    /*
        等待线程运行完成
    */
    void join();

    /*
        获取当前线程
    */
    static Threading* GetThis();

    /*
        获取线程名
    */
    static const std::string& GetName();

    /*
        设置线程名
    */
    static void SetName( const std::string& name );

    /*
        获取线程 id
    */
    pid_t get_id() { return this->m_id; }

private:
    /*
        运行线程
    */
    static void* run( void* arg );

private:
    /* 删除一些函数 */
    Threading( Threading& )  = delete;
    Threading( Threading&& ) = delete;
    void operator=( Threading& ) = delete;

private:
    pid_t m_id;                   /* 线程id */
    std::string m_name;           /* 线程名 */
    pthread_t m_thread = 0;       /* 线程 */
    std::function< void() > func; /* 线程执行函数 */
    sem_t m_sem;                 /* 信号量 */
};
}

#endif