#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <semaphore.h>
#include <vector>

#include "../log/log.h"
#include "../thread/thread.h"

namespace star
{

static thread_local Threading::ptr m_thread; /* 调度器运行线程 */
static thread_local size_t find_begin     = 0;
static thread_local size_t find_end       = 0;
static thread_local std::string find_name = "";
static thread_local int result_index      = -1;
static void* self                         = nullptr; /* this 指针 */
static pid_t m_id                         = 0;
/* 全局变量 */
extern std::vector< void* > Schedule_args;
extern int Schedule_answer;
extern pthread_mutex_t mutex;

/* 调度器 */
class Scheduler
{
public:
    enum Task_Status
    {
        FINISH     = 0,
        NOT_FINISH = 1,
        FAILED     = 2,
        pause      = 3,
        INIT       = 4,
        RUNING     = 5
    };

    struct task
    {
        task( std::function< void() > func, std::string t_name, Task_Status t_status = INIT )
        {
            this->t_func   = func;
            this->t_name   = t_name;
            this->t_status = t_status;
        }

        bool operator==( std::string name )
        {
            if ( name == this->t_name )
            {
                return true;
            }
            return false;
        }

        std::function< void() > t_func;
        std::string t_name;
        Task_Status t_status;
    };

public:
    typedef std::shared_ptr< Scheduler > ptr;
    Scheduler( size_t max_threads = 5, size_t max_fibers = 5 )
    {
        this->max_threads = max_threads;
        this->max_fibers  = max_fibers;
        m_logger.reset( STAR_NAME( "SCHEDULER" ) );
    }
    ~Scheduler() = default;

private:
    /* 查找的子任务 */
    static void find_value();

public:
    /*
        注册一个任务
     */
    virtual void Regist_task( std::function< void() > t_func, std::string t_name );

    /*
        分配一个任务
     */
    virtual void assign_task( task& t_task );

    /*
        删除一个任务
     */
    virtual void delete_task( std::string t_name );

    /*
        重设任务
     */
    virtual void reset_task( std::string t_name, std::function< void() > t_func );

    /* 获取信号量 */
    // sem_t get_sem() { return m_thread->get_sem(); }

    /* 调度 */
    void manage();

protected:
    size_t max_fibers;                       /* 最大协程数 */
    size_t max_threads;                      /* 最大线程数 */
    std::deque< Scheduler::task > m_tasks;   /* 任务池 */
    std::vector< Threading::ptr > m_threads; /* 线程池 */
    Logger::ptr m_logger;                    /* 日志器 */
    int16_t thread_free_time;
};

static thread_local std::deque< Scheduler::task > arr;

}

#endif