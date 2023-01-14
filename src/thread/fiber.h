/**
 * author: SongZihui-sudo
 * file: fiber.h
 * desc: 协程模块，简单封装一下libaco协程库
 * date: 2022-12-24
 */

#ifndef FIBER_H
#define FIBER_H

#include <aco.h>
#include <functional>
#include <memory>
#include <ucontext.h>

namespace star
{

/* 初始化环境 */
#define FILEBER_INIT() aco_thread_init( NULL );

class fiber
{
public:
    enum State
    {
        INIT,  /* 初始化状态 */
        HOLD,  /* 暂停状态 */
        EXEC,  /* 执行中状态 */
        TERM,  /* 结束状态 */
        READY, /* 可执行状态 */
        EXCEPT /* 异常状态 */
    };

    typedef std::shared_ptr< fiber > ptr;

public:
    /* 构建 main_co */
    fiber()
    {
        aco_thread_init(NULL);
        this->m_fiber  = aco_create( NULL, NULL, 0, NULL, NULL );
        this->isMainco = true;
    }
    fiber( std::function< void() > cb, size_t stacksize = 0 );
    fiber( aco_t* in_fib ) { this->m_fiber = in_fib; }
    ~fiber()
    {
        /* 销毁协程和协程栈 */
        if ( this->m_stack )
        {
            aco_share_stack_destroy( this->m_stack );
        }
        if ( this->m_fiber )
        {
            aco_destroy( this->m_fiber );
        }
    };

public:
    /* 是否主协程 */
    bool isMainFiber() { return this->isMainco; }

    /* 重置协程 */
    void reset( std::function< void() > cb, size_t m_stacksize );

    /* 获取协程id */
    uint64_t getId() const { return m_id; }

    /* 获取协程状态 */
    State getState() const { return m_state; }

    /* 从调用者处Yield出来并开始或者继续协程co的执行 */
    void resume();

    /* 从调用者co处Yield出来并且Resume co->main_co的执行 */
    void yield();

    /* 除了与 yield()一样的功能之外，aco_exit()会另外设置co->is_end为1，以标志co的状态为 "end" */
    void back();

    /* 获取协程运行栈 */
    aco_share_stack_t* get_stack() { return this->m_stack; }

    /* 协程指针 */
    aco_t* getThis() { return this->m_fiber; }

public:
    /* 设置主协程 */
    static void SetThis( fiber::ptr f );

    /* 获取主协程 */
    static fiber::ptr GetMainFiber();

    /* 获取当前协程 */
    static aco_t* getCurrentFiber();

    /* 获取协程id */
    static uint64_t GetMainFiberId();

    State m_state = INIT; /* 协程状态 */

    void operator=( aco_t* in_fiber ) { this->m_fiber = in_fiber; }

private:
    bool isMainco              = false;   /* 是否为主协程 */
    uint64_t m_id              = 0;       /* 协程id */
    uint32_t m_stacksize       = 0;       /* 协程运行栈大小 */
    aco_share_stack_t* m_stack = nullptr; /* 协程运行栈指针 */
    aco_t* m_fiber;                       /* 协程对象 */
    std::function< void() > m_cb;         /* 协程运行函数 */
};
}

#endif