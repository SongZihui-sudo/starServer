#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <functional>
#include <memory>
#include <semaphore.h>
#include <vector>

#include "../log/log.h"
#include "../thread/thread.h"

/* 插入消息 */
template< typename T1, typename... Ts >
void Register( std::vector< std::vector< void* > >& obj, int index, T1 arg1, Ts... arg_left )
{

    T1* temp = new T1;
    *temp    = arg1;
    obj[index].push_back( temp );

    if constexpr ( sizeof...( arg_left ) > 0 )
    {
        Register( obj, index, arg_left... );
    }
}

/* 生成消息映射 */
static std::vector< std::vector< void* > > GENERATER_MESSAGE_MAP( size_t len )
{
    std::vector< std::vector< void* > > ret;

    for ( size_t i = 0; i < len; i++ )
    {
        ret.push_back( std::vector< void* >() );
    }

    return ret;
}

/* 宏定义 */
#define MESSAGE_MAP( obj, len )                                                            \
    std::vector< std::vector< void* > > obj = GENERATER_MESSAGE_MAP( len );

#define ADD_MESSAGE_MAP_LEN( obj ) obj.push_back( std::vector< void* >() );

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

    /* 运行调度器 */
    virtual void run();

    /* 获取信号量 */
    //sem_t get_sem() { return m_thread->get_sem(); }

    /* 动态调度 */
    void manage();

protected:
    /*
        启动静态调度器
    */
    static void start();

protected:
    size_t max_fibers;                       /* 最大协程数 */
    size_t max_threads;                      /* 最大线程数 */
    std::vector< Scheduler::task > m_tasks;  /* 任务池 */
    std::vector< Threading::ptr > m_threads; /* 线程池 */
    Logger::ptr m_logger;                    /* 日志器 */
};

static thread_local std::vector< Scheduler::task > arr;

}

#endif