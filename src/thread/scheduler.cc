/*
 * author: SongZihui-sudo
 * file: scheduler.cc
 * desc: 调度器模块
 * date: 2023-1-12
 */

#include "./scheduler.h"
#include "../common/common.h"
#include "../log/log.h"
#include "../setting/setting.h"

#include <aco.h>
#include <functional>
#include <memory>
#include <mutex>

namespace star
{
static star::Logger::ptr g_logger( STAR_NAME( "scheduler" ) );

static thread_local Scheduler* t_scheduler       = nullptr;
static thread_local fiber::ptr t_scheduler_fiber = nullptr;

Scheduler::Scheduler( size_t threads, bool use_caller, const std::string& name )
: m_name( name )
{
    STAR_ASSERT( threads > 0, g_logger );

    if ( use_caller )
    {
        aco_get_co();
        --threads;

        STAR_ASSERT( GetThis() == nullptr, g_logger );
        t_scheduler = this;

    std:
        m_rootFiber->reset( std::bind( &Scheduler::run, this ),
                            globeConfig->get< std::string >( "Fiber" )["StackSize"].asInt() );
        star::Threading::SetName( m_name );

        t_scheduler_fiber = m_rootFiber;
        m_rootThread      = GetThreadId();
        m_threadIds.push_back( m_rootThread );
    }
    else
    {
        m_rootThread = -1;
    }
    m_threadCount = threads;
}

Scheduler::~Scheduler()
{
    STAR_ASSERT( m_stopping, g_logger );
    if ( GetThis() == this )
    {
        t_scheduler = nullptr;
    }
}

Scheduler* Scheduler::GetThis() { return t_scheduler; }

fiber::ptr Scheduler::GetMainFiber() { return t_scheduler_fiber; }

void Scheduler::start()
{
    this->m_mutex.lock();
    if ( !m_stopping )
    {
        return;
    }
    m_stopping = false;
    STAR_ASSERT( m_threads.empty(), g_logger );

    m_threads.resize( m_threadCount );
    for ( size_t i = 0; i < m_threadCount; ++i )
    {
        m_threads[i].reset( new Threading( std::bind( &Scheduler::run, this ),
                                           m_name + "_" + std::to_string( i ) ) );
        m_threadIds.push_back( m_threads[i]->get_id() );
    }
    this->m_mutex.unlock();
}

void Scheduler::stop()
{
    m_autoStop = true;
    if ( m_rootFiber && m_threadCount == 0
         && ( m_rootFiber->getState() == fiber::TERM || m_rootFiber->getState() == fiber::INIT ) )
    {
        INFO_STD_STREAM_LOG( g_logger ) << this->getName() << " stopped";
        m_stopping = true;

        if ( stopping() )
        {
            return;
        }
    }

    // bool exit_on_this_fiber = false;
    if ( m_rootThread != -1 )
    {
        STAR_ASSERT( GetThis() == this, g_logger );
    }
    else
    {
        STAR_ASSERT( GetThis() != this, g_logger );
    }

    m_stopping = true;
    for ( size_t i = 0; i < m_threadCount; ++i )
    {
        tickle();
    }

    if ( m_rootFiber )
    {
        tickle();
    }

    if ( m_rootFiber )
    {
        if ( !stopping() )
        {
            this->m_rootFiber->resume();
        }
    }

    std::vector< Threading::ptr > thrs;
    {
        m_mutex.lock();
        thrs.swap( m_threads );
    }

    for ( auto& i : thrs )
    {
        i->join();
    }
}

void Scheduler::switchTo( int thread )
{
    STAR_ASSERT( Scheduler::GetThis() != nullptr, g_logger );
    if ( Scheduler::GetThis() == this )
    {
        if ( thread == -1 || thread == GetThreadId() )
        {
            return;
        }
    }

    fiber::ptr temp( new fiber( fiber::getCurrentFiber() ) );
    schedule( temp, thread );
    aco_yield1( fiber::getCurrentFiber() );
}

std::ostream& Scheduler::dump( std::ostream& os )
{
    os << "[Scheduler name=" << m_name << " size=" << m_threadCount << " active_count=" << m_activeThreadCount
       << " idle_count=" << m_idleThreadCount << " stopping=" << m_stopping << " ]" << std::endl
       << "    ";
    for ( size_t i = 0; i < m_threadIds.size(); ++i )
    {
        if ( i )
        {
            os << ", ";
        }
        os << m_threadIds[i];
    }
    return os;
}

void Scheduler::run()
{
    DEBUG_STD_STREAM_LOG( g_logger ) << m_name << " run";
    setThis();

    this->m_mutex.lock();
    if ( !t_scheduler_fiber )
    {
        fiber::ptr f( new fiber() );
        t_scheduler_fiber->SetThis( f );
    }
    this->m_mutex.unlock();

    if ( GetThreadId() != m_rootThread )
    {
        if ( fiber::getCurrentFiber() )
        {
            *t_scheduler_fiber = fiber::getCurrentFiber();
        }
    }

    fiber::ptr idle_fiber( new fiber( std::bind( &Scheduler::idle, this ) ) );
    fiber::ptr cb_fiber;

    FiberAndThread ft;
    while ( true )
    {
        ft.reset();
        bool tickle_me = false;
        bool is_active = false;
        {
            this->m_mutex.lock();
            auto it = m_fibers.begin();
            while ( it != m_fibers.end() )
            {
                if ( it->thread != -1 && it->thread != GetThreadId() )
                {
                    ++it;
                    tickle_me = true;
                    continue;
                }

                STAR_ASSERT( it->_fiber || it->cb, g_logger );
                if ( it->_fiber && it->_fiber->getState() == fiber::EXEC )
                {
                    ++it;
                    continue;
                }

                ft = *it;
                m_fibers.erase( it++ );
                ++m_activeThreadCount;
                is_active = true;
                break;
            }
            tickle_me |= it != m_fibers.end();
        }

        if ( tickle_me )
        {
            tickle();
        }

        if ( ft._fiber && ( ft._fiber->getState() != fiber::TERM && ft._fiber->getState() != fiber::EXCEPT ) )
        {
            ft._fiber->resume();
            --m_activeThreadCount;

            if ( ft._fiber->getState() == fiber::READY )
            {
                schedule( ft._fiber );
            }
            else if ( ft._fiber->getState() != fiber::TERM && ft._fiber->getState() != fiber::EXCEPT )
            {
                ft._fiber->m_state = fiber::HOLD;
            }
            ft.reset();
        }
        else if ( ft.cb )
        {
            if ( cb_fiber )
            {
                cb_fiber->reset( ft.cb,
                                 globeConfig->get< std::string >( "Fiber" )["StackSize"].asInt() );
            }
            else
            {
                cb_fiber.reset( new fiber( ft.cb ) );
            }
            ft.reset();
            cb_fiber->resume();
            --m_activeThreadCount;
            if ( cb_fiber->getState() == fiber::READY )
            {
                schedule( cb_fiber );
                cb_fiber.reset();
            }
            else if ( cb_fiber->getState() == fiber::EXCEPT || cb_fiber->getState() == fiber::TERM )
            {
                cb_fiber->reset( nullptr,
                                 globeConfig->get< std::string >( "Fiber" )["StackSize"].asInt() );
            }
            else
            { // if(cb_fiber->getState() != Fiber::TERM) {
                cb_fiber->m_state = fiber::HOLD;
                cb_fiber.reset();
            }
        }
        else
        {
            if ( is_active )
            {
                --m_activeThreadCount;
                continue;
            }
            if ( idle_fiber->getState() == fiber::TERM )
            {
                INFO_STD_STREAM_LOG( g_logger ) << "idle fiber term" << "%n%0";
                break;
            }

            ++m_idleThreadCount;
            idle_fiber->resume();
            --m_idleThreadCount;
            if ( idle_fiber->getState() != fiber::TERM && idle_fiber->getState() != fiber::EXCEPT )
            {
                idle_fiber->m_state = fiber::HOLD;
            }
        }
    }
}

bool Scheduler::stopping()
{
    this->m_mutex.lock();
    return m_autoStop && m_stopping && m_fibers.empty() && m_activeThreadCount == 0;
}

void Scheduler::idle()
{
    INFO_STD_STREAM_LOG( g_logger ) << "idle";
    while ( !stopping() )
    {
        aco_yield1( star::fiber::getCurrentFiber() );
    }
}

void Scheduler::setThis() { t_scheduler = this; }

void Scheduler::tickle() { INFO_STD_STREAM_LOG( g_logger ) << "tickle"; }

}

#include "../setting/setting.cc"