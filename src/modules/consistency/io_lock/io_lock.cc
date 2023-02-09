#include "./io_lock.h"
#include "modules/log/log.h"
#include "modules/meta_data/meta_data.h"

/*
    一个文件上读锁后，不可以在写，还可以读
    一个文件上写锁后，不可以写，不可以读
 */

namespace star
{

static Logger::ptr g_logger( STAR_NAME( "chunk_lock_logger" ) );

void io_lock::lock_read( file_operation flag )
{
    if ( flag != read )
    {
        int i = 0;
        /* 阻塞当前的线程 */
        while ( true )
        {
            if ( this->m_signal == read_unlock || this->m_signal == none_lock || this->m_signal == write_unlock )
            {    
                break;
            }
            if ( !i )
            {
                DEBUG_STD_STREAM_LOG( g_logger )
                << "%D"
                << "chunk has been locked read!" << Logger::endl();
            }
            i++;
        }
    }
    /* 上读锁 */
    this->m_signal = read_lock;
    this->flag     = true;
}

void io_lock::lock_write( file_operation flag )
{
    int i = 0;
    /* 上锁后读和写都不能在进行，等待 */
    while ( true )
    {
        if ( this->m_signal == write_unlock || this->m_signal == none_lock || this->m_signal == read_unlock )
        {
            break;
        }
        if ( !i )
        {
            DEBUG_STD_STREAM_LOG( g_logger ) << "%D"
                                             << "chunk has been locked write!" << Logger::endl();
        }
        i++;
    }
    /* 在上锁 */
    this->m_signal = write_lock;
    this->flag     = true;
}

void io_lock::release_read()
{
    this->m_signal = read_unlock;
    this->flag     = false;
}

void io_lock::release_write()
{
    this->m_signal = write_unlock;
    this->flag     = false;
}
}
