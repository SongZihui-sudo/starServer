#include "lease.h"
#include "modules/Scheduler/mods/timer/timer.h"
#include "modules/log/log.h"
#include <asm-generic/errno.h>
#include <cstdint>

namespace star
{

static Logger::ptr g_logger( STAR_NAME( "lease_logger" ) );
static std::string lease_id = "";
bool* lease_flag;

lease::lease( int32_t lease_times )
{
    this->m_id      = random_string( 16 );
    this->m_secends = lease_times;
    this->m_timer.reset( new Timer( this->lease_invalid, lease_times ) );
    /* 启动一个定时器 */
    this->m_timer->run();
    lease_flag = &this->available;
}

void lease::renew()
{
    this->available = true;
    /* 重新启动定时器 */
    this->m_timer.reset( new Timer( this->lease_invalid, this->m_secends ) );
}

void lease::lease_invalid() { *lease_flag = false; }

/* 授权新租约 */
void lease_manager::new_lease()
{
    lease::ptr new_lease( new lease( this->default_lease_time ) );
    this->lease_tab[new_lease->get_id()] = new_lease;
}

/* 销毁过期读租约 */
void lease_manager::destory_lease( std::string id )
{

    if ( this->lease_tab.erase( id ) )
    {
        DEBUG_STD_STREAM_LOG( g_logger ) << "%D"
                                         << "Remove Lease Successfully!"
                                         << "%n%0";
    }
    else
    {
        DEBUG_STD_STREAM_LOG( g_logger ) << "%D"
                                         << "Remove Lease fail!"
                                         << "%n%0";
    }
}

/* 续约 */
void lease_manager::renew_lease( std::string id )
{
    if ( this->lease_tab[id] )
    {
        this->lease_tab[id]->renew();
        DEBUG_STD_STREAM_LOG( g_logger ) << "%D"
                                         << "Renew Lease Successfully!"
                                         << "%n%0";
    }
    DEBUG_STD_STREAM_LOG( g_logger ) << "%D"
                                     << "Renew Lease fail!"
                                     << "%n%0";
}

/* 检查租约是不是全部过期 */
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

}