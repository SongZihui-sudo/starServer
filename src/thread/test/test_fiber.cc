#include "../../log/log.h"
#include "../fiber.h"
#include "../thread.h"

#include <aco.h>
#include <iostream>
#include <vector>

star::Logger::ptr g_logger( STAR_NAME( "g_logger" ) );

void run_in_fiber()
{
    INFO_STD_STREAM_LOG( g_logger ) << "run_in_fiber begin" << "%n%0";
    aco_yield();

    INFO_STD_STREAM_LOG( g_logger ) << "run_in_fiber end" << "%n%0";
    aco_yield();
}

void test_fiber()
{
    INFO_STD_STREAM_LOG( g_logger ) << "main begin -1"
                                    << "%n%0";
    {
        INFO_STD_STREAM_LOG( g_logger ) << "main begin"
                                        << "%n%0";
        star::fiber::ptr test( new star::fiber( run_in_fiber ) );

        test->resume();

        INFO_STD_STREAM_LOG( g_logger ) << "main after end"
                                        << "%n%0";
        test->back();
    }
    INFO_STD_STREAM_LOG( g_logger ) << "main after end2" << "%n%0";
}

int main()
{
    star::Threading::SetName( "main" );

    std::vector< star::Threading::ptr > thrs;
    for ( int i = 0; i < 1; i++ )
    {
        thrs.push_back( star::Threading::ptr(
        new star::Threading( &test_fiber, "name_" + std::to_string( i ) ) ) );
    }
    for ( auto i : thrs )
    {
        i->join();
    }
    return 0;
}