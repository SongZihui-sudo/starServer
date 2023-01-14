#include "../scheduler.h"
#include "../../common/common.h"

#include <string>

star::Logger::ptr g_logger( STAR_NAME( "g_logger" ) );

void test_fiber()
{
    static int s_count = 5;
    INFO_STD_STREAM_LOG( g_logger ) << "test in fiber s_count=" << std::to_string(s_count);

    sleep( 1 );
    if ( --s_count >= 0 )
    {
        star::Scheduler::GetThis()->schedule( &test_fiber, GetThreadId() );
    }
}

int main()
{
    INFO_STD_STREAM_LOG( g_logger ) << "main" << "%n%0";
    star::Scheduler sc( 3, false, "test" );
    sc.start();
    sleep( 2 );
    INFO_STD_STREAM_LOG( g_logger ) << "schedule" << "%n%0";
    sc.schedule( &test_fiber );
    sc.stop();
    INFO_STD_STREAM_LOG( g_logger ) << "over" << "%n%0";
    return 0;
}