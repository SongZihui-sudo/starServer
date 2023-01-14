#include "./fiber.h"
#include "../log/log.h"

#include <atomic>
#include <functional>
#include <memory>

namespace star
{
static Logger::ptr g_logger( STAR_NAME( "Fiber" ) );

extern thread_local fiber::ptr t_fiber       = nullptr;

static thread_local size_t Fiber_count = 0;

fiber::fiber( std::function< void() > cb, size_t stacksize )
{
    this->m_stacksize = stacksize;
    this->m_cb        = cb;
    void ( *func )()  = nullptr;
    std::bind( cb, func );
    if ( t_fiber )
    {
        /* 创建携程 */
        this->m_stack = aco_share_stack_new(0);
        this->m_fiber = aco_create( fiber::GetMainFiber()->m_fiber, this->m_stack, 0, func, NULL );
    }
    else
    {
        ERROR_STD_STREAM_LOG( g_logger )
        << "Master Ctrip has not been created yet. A main Ctrip will be created!" << "%n%0";
        aco_thread_init(func);
        /* 设置主协程 */
        this->m_fiber = aco_create( NULL, NULL, 0, NULL, NULL );
        t_fiber.reset(this);  
        this->isMainco   = true;
    }
    this->m_id = Fiber_count;
    Fiber_count++;
}

static uint64_t GetMainFiberId()
{
    if ( t_fiber )
    {
        return t_fiber->getId();
    }
    return 0;
}

fiber::ptr GetMainFiber()
{
    if ( t_fiber )
    {
        return t_fiber;
    }
    return nullptr;
}

void fiber::reset( std::function< void() > cb, size_t m_stacksize )
{
    STAR_ASSERT( this->m_stack, g_logger );
    STAR_ASSERT( this->m_fiber, g_logger );
    /* 销毁协程 */
    aco_destroy( this->m_fiber );
    /* 销毁栈 */
    aco_share_stack_destroy( this->m_stack );
    m_cb              = cb; /* 重置任务 */
    this->m_stacksize = m_stacksize;
    /* 分配栈 */
    this->m_stack = aco_share_stack_new( this->m_stacksize );
    /* 创建协程 */
    void ( *func )() = nullptr;
    std::bind( cb, func );
    this->m_fiber = aco_create( fiber::GetMainFiber()->m_fiber, this->m_stack, 0, func, NULL );

    m_state = INIT; /* 重置状态 */
}

void fiber::SetThis( fiber::ptr f )
{
    if ( t_fiber )
    {
        t_fiber = f;
    }
    else
    {
        ERROR_FILE_STREAM_LOG( g_logger ) << "Master Ctrip has not been created yet. %n%0";
    }
}

fiber::ptr fiber::GetMainFiber()
{
    if ( t_fiber )
    {
        return t_fiber;
    }
    ERROR_FILE_STREAM_LOG( g_logger ) << "Master Ctrip has not been created yet. %n%0";

    return nullptr;
}

void fiber::resume()
{
    if ( this->m_fiber && this->isMainFiber() )
    {
        aco_resume( this->m_fiber );
    }
    this->m_state = READY;
}

void fiber::yield()
{
    if ( this->m_fiber && !this->isMainFiber() )
    {
        aco_yield();
    }

    this->m_state = READY;
}

aco_t* fiber::getCurrentFiber() { return aco_get_co(); }

void fiber::back()
{
    if ( this->m_fiber )
    {
        aco_exit();
    }

    this->m_state = TERM;
}

uint64_t fiber::GetMainFiberId()
{
    if ( t_fiber )
    {
        return t_fiber->m_id;
    }
    ERROR_FILE_STREAM_LOG( g_logger ) << "Master Ctrip has not been created yet. %n%0";

    return -1;
}

}