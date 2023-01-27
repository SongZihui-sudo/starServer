
#include "modules/Scheduler/mods/timer/timer.h"
#include "modules/log/log.h"

star::Logger::ptr g_logger( STAR_NAME( "timer_test_logger" ) );

void foo()
{
    INFO_STD_STREAM_LOG( g_logger ) << "Time Test!"
                                    << "%n%0";
}

int main()
{
    star::Timer::ptr timer_test( new star::Timer( foo, 2 ) );

    timer_test->run();

    while ( true )
    {
    
    }
    return 0;
}