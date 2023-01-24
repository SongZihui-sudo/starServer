/*
 * author: SongZihui-sudo
 * file: thead.cc
 * desc: 线程模块
 * date: 2023-1-4
 */

#include "./thread.h"
#include "../common/common.h"
#include "../log/log.h"

#include <cstddef>
#include <exception>
#include <functional>
#include <string>
#include <unistd.h>

namespace star
{

int thread_nums = 0;

Threading::Threading( std::function< void() > func, const std::string& name )
{
    this->m_name = name;
    this->func   = func;
    if ( name.empty() )
    {
        m_name = "UNKNOW";
    }

    this->t_logger.reset( STAR_NAME( "THREAD_LOGGER" ) );

    m_status = INIT;

    thread_nums++;

    INFO_STD_STREAM_LOG( this->t_logger )
    << std::to_string( getTime() ) << " <----> "
    << "New Thread Created. Current nums of thread: " << std::to_string( thread_nums ) << "%n%0";

    int rt = pthread_create( &m_thread, nullptr, &Threading::run, this );

    INFO_STD_STREAM_LOG( this->t_logger ) << std::to_string( getTime() ) << " <----> "
                                          << "New Thread Created!"
                                          << "flag: " << std::to_string( rt ) << "%n%0";

    if ( rt )
    {
        m_status = ERROR;
        ERROR_STD_STREAM_LOG( this->t_logger )
        << "pthread_create thread fail, rt=" << S( rt ) << " name=" << name << "%n%0";
        throw std::logic_error( "pthread_create error" );
    }

    // sem_wait( &this->m_sem );
}

void Threading::join()
{
    if ( this->m_thread )
    {
        this->m_status = RUNING;

        int rt = pthread_join( this->m_thread, nullptr );

        if ( rt )
        {
            m_status = ERROR;
            ERROR_STD_STREAM_LOG( this->t_logger )
            << "pthread_join thread fail, rt=" << S( rt ) << " name=" << m_name << "%n%0";
            throw std::logic_error( "pthread_join error" );
        }

        m_status = FREE;
        return;
    }
    ERROR_STD_STREAM_LOG( this->t_logger ) << "Error! What: The thread has not created.!"
                                           << "%n%0";
}

void Threading::exit()
{
    /* 退出线程 */
    pthread_exit( NULL );
    m_status = FREE;
}

void* Threading::run( void* arg )
{
    Threading* thread = ( Threading* )arg;

    INFO_STD_STREAM_LOG( thread->t_logger )
    << std::to_string( getTime() ) << " <----> "
    << "Thread " << std::to_string( thread->m_id ) << " " << thread->get_name() << " Runing"
    << "%n%0";

    thread->m_status = RUNING;
    thread->m_id     = GetThreadId();
    pthread_setname_np( pthread_self(), thread->get_name().substr( 0, 15 ).c_str() );

    try
    {
        std::function< void() > func;
        func.swap( thread->func );
        func();
    }
    catch ( std::exception& e )
    {
        throw e.what();
    }

    thread->m_status = FREE;

    INFO_STD_STREAM_LOG( thread->t_logger )
    << std::to_string( getTime() ) << " <----> "
    << "Thread " << std::to_string( thread->m_id ) << " " << thread->get_name() << " Exit!"
    << "%n%0";

    // sem_post( &thread->m_sem );

    return nullptr;
}

void Threading::reset( std::function< void() > func, std::string thread_name )
{
    this->m_name = thread_name;
    this->func   = func;

    m_status = INIT;

    int rt = pthread_create( &m_thread, nullptr, &Threading::run, this );

    if ( rt )
    {
        m_status = ERROR;
        ERROR_STD_STREAM_LOG( this->t_logger )
        << "pthread_create thread fail, rt=" << S( rt ) << " name=" << this->m_name << "%n%0";
        throw std::logic_error( "pthread_create error" );
    }

    // sem_wait( &this->m_sem );
}

}