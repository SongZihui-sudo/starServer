#include "lease.h"
#include "modules/Scheduler/mods/timer/timer.h"
#include "modules/log/log.h"
#include "modules/thread/thread.h"
#include <asm-generic/errno.h>
#include <cstdint>
#include <string>

namespace star
{

static Logger::ptr g_logger( STAR_NAME( "lease_logger" ) );
static std::string lease_id = "";
bool lease::available       = false;

lease::lease( int32_t lease_times )
{
    this->m_id      = random_string( 16 );
    this->m_secends = lease_times;
}

void lease::start()
{
    this->m_timer.reset( new Timer( this->lease_invalid, this->m_secends ) );
    /* 启动一个定时器 */
    this->m_timer->run();
    this->available = true;
}

void lease::renew()
{
    if ( this->available )
    {
        /* 停止定时器 */
        this->m_timer->interrupt();
        /* 重新启动定时器 */
        this->m_timer.reset( new Timer( this->lease_invalid, this->m_secends ) );
        this->available = true;
    }
    else
    {
        WERN_STD_STREAM_LOG( g_logger )
        << "The lease has expired and cannot be renewed." << Logger::endl();
    }
}

void lease::lease_invalid()
{
    DEBUG_STD_STREAM_LOG( g_logger ) << "Lease is late!" << Logger::endl();
    lease::available = false;
}

void lease_manager::new_lease( std::string file_id, std::string client_id )
{
    lease::ptr new_lease( new lease( this->default_lease_time ) );
    this->lease_tab[new_lease->get_id()] = new_lease;
    this->m_client_to_lease[client_id]   = new_lease;
    this->m_file_to_lease[file_id]       = new_lease->get_id();
    this->m_lease_id__to_file[new_lease->get_id()].push_back( file_id );
    this->lease_to_client[new_lease->get_id()] = client_id;
    new_lease->start();
}

void lease_manager::destory_lease( std::string lease_id )
{

    if ( this->lease_tab.erase( lease_id ) )
    {
        this->m_lease_id__to_file.erase( lease_id );
        this->lease_to_client.erase( lease_id );
        std::string client_id = this->lease_to_client[lease_id];
        this->lease_to_client.erase( lease_id );
        this->m_client_to_lease.erase( client_id );
        DEBUG_STD_STREAM_LOG( g_logger ) << "%D"
                                         << "Remove Lease Successfully!" << Logger::endl();
        BREAK( g_logger );
        return;
    }

    DEBUG_STD_STREAM_LOG( g_logger ) << "%D"
                                     << "Remove Lease fail!" << Logger::endl();
}

void lease_manager::renew_lease( std::string id )
{
    if ( this->lease_tab[id] )
    {
        this->lease_tab[id]->renew();
        DEBUG_STD_STREAM_LOG( g_logger ) << "%D"
                                         << "Renew Lease Successfully!" << Logger::endl();
    }
    DEBUG_STD_STREAM_LOG( g_logger ) << "%D"
                                     << "Renew Lease fail!" << Logger::endl();
}

bool lease_manager::is_all_late()
{
    for ( auto item : this->lease_tab )
    {
        if ( item.second->is_available() )
        {
            return false;
        }
    }
    return true;
}

void lease_manager::destory_invalid_lease()
{
    if ( this->lease_tab.empty() )
    {
        return;
    }
    for ( auto item : this->lease_tab )
    {
        if ( !item.second->is_available() && item.second->get_status() == Threading::Thread_Status::FREE )
        {
            this->destory_lease( item.first );
        }
    }
    BREAK( g_logger );
}

}