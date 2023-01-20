#include "../../log/log.h"
#include "../../thread/thread.h"
#include "../scheduler.h"

#include <any>
#include <iostream>
#include <semaphore.h>
#include <unistd.h>
#include <vector>

star::Logger::ptr cur_logger( STAR_NAME( "CUR_LOGGER" ) );

void task1()
{
    INFO_STD_STREAM_LOG( cur_logger ) << "task 1"
                                      << "%n%0";
}

void task2()
{
    INFO_STD_STREAM_LOG( cur_logger ) << "task 2"
                                      << "%n%0";
}

void task3()
{
    INFO_STD_STREAM_LOG( cur_logger ) << "task 3"
                                      << "%n%0";
}

void task4()
{
    INFO_STD_STREAM_LOG( cur_logger ) << "task 4"
                                      << "%n%0";
}

void task5()
{
    INFO_STD_STREAM_LOG( cur_logger ) << "task 5"
                                      << "%n%0";
}

int main()
{
    star::Scheduler::ptr test( new star::Scheduler() );

    test->run();

    int i = 0;
    MESSAGE_MAP( arg_map, 14 );

    Register( arg_map, 0, 4 );
    Register( arg_map, 1, 1, std::function< void() >( task1 ), "task1" );
    Register( arg_map, 2, 1, std::function< void() >( task2 ), "task2" );
    Register( arg_map, 3, 1, std::function< void() >( task3 ), "task3" );
    Register( arg_map, 4, 1, std::function< void() >( task4 ), "task4" );
    Register( arg_map, 5, 4 );

    Register( arg_map, 6, 1, std::function< void() >( task5 ), "task5" );
    Register( arg_map, 7, 1, std::function< void() >( task1 ), "task6" );
    Register( arg_map, 8, 1, std::function< void() >( task2 ), "task7" );
    Register( arg_map, 9, 1, std::function< void() >( task3 ), "task8" );
    Register( arg_map, 10, 4 );

    Register( arg_map, 11, 3, std::function< void() >( task5 ), "task7" );

    Register( arg_map, 12, 4 );

    Register( arg_map, 13, 2, "task5" );

    Register( arg_map, 14, 5 );

    if ( star::Schedule_args.empty() )
    {
        /* 取指令 */
        star::Schedule_args = arg_map[i];
        i++;
    }

    while ( true )
    {
        if ( star::Schedule_answer == 1 )
        {
            /* 取指令 */
            star::Schedule_args.clear();
            star::Schedule_args   = arg_map[i];
            star::Schedule_answer = 0;
            i++;
        }
        else if ( star::Schedule_answer == 2 )
        {
            star::Schedule_answer = 0;
            return 0;
        }
    }

    return 0;
}