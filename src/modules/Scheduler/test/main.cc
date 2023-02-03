#include "../../log/log.h"
#include "../../thread/thread.h"
#include "../scheduler.h"

#include <iostream>
#include <semaphore.h>
#include <unistd.h>
#include <vector>

star::Logger::ptr cur_logger( STAR_NAME( "CUR_LOGGER" ) );

void task1()
{
    INFO_STD_STREAM_LOG( cur_logger ) << "task 1"
                                      << Logger::endl();
}

void task2()
{
    INFO_STD_STREAM_LOG( cur_logger ) << "task 2"
                                      << Logger::endl();
}

void task3()
{
    INFO_STD_STREAM_LOG( cur_logger ) << "task 3"
                                      << Logger::endl();
}

void task4()
{
    INFO_STD_STREAM_LOG( cur_logger ) << "task 4"
                                      << Logger::endl();
}

void task5()
{
    INFO_STD_STREAM_LOG( cur_logger ) << "task 5"
                                      << Logger::endl();
}

int main()
{
    star::Scheduler::ptr test( new star::Scheduler() );
    return 0;
}