#include "../io_lock.h"
#include "modules/log/log.h"
#include "modules/meta_data/meta_data.h"
#include "modules/thread/thread.h"
#include <cstddef>
#include <cstdint>
#include <vector>

star::Logger::ptr g_logger( STAR_NAME( "chunk_test_logger" ) );
static int16_t index = 0;

void test_case1()
{
    star::io_lock::ptr lock( new star::io_lock() );

    lock->lock_read( star::read );

    DEBUG_STD_STREAM_LOG( g_logger ) << "Locked!"
                                     << "index: " << S( index ) << star::Logger::endl();

    index++;
    lock->release_read();
}

void test_case2()
{
    star::io_lock::ptr lock( new star::io_lock() );

    lock->lock_read( star::write );

    DEBUG_STD_STREAM_LOG( g_logger ) << "Locked!"
                                     << "index: " << S( index ) << star::Logger::endl();

    index++;
    lock->release_read();
}

void test_case3()
{
    star::io_lock::ptr lock( new star::io_lock() );

    lock->lock_write( star::read );

    DEBUG_STD_STREAM_LOG( g_logger ) << "Locked!"
                                     << "index: " << S( index ) << star::Logger::endl();

    index++;
    lock->release_write();
}

void test_case4()
{
     star::io_lock::ptr lock( new star::io_lock() );

    lock->lock_write( star::write );

    DEBUG_STD_STREAM_LOG( g_logger ) << "Locked!"
                                     << "index: " << S( index ) << star::Logger::endl();

    index++;
    lock->release_write();
}

int main()
{
    std::vector< star::Threading::ptr > threads;
    for ( size_t i = 0; i < 5; i++ )
    {
        star::Threading::ptr new_thread(
        new star::Threading( test_case1, "chunk_lock_test" ) );
        threads.push_back( new_thread );
    }

    for ( size_t i = 0; i < 5; i++ )
    {
        star::Threading::ptr new_thread(
        new star::Threading( test_case2, "chunk_lock_test" ) );
        threads.push_back( new_thread );
    }

    for ( size_t i = 0; i < 5; i++ )
    {
        star::Threading::ptr new_thread(
        new star::Threading( test_case3, "chunk_lock_test" ) );
        threads.push_back( new_thread );
    }

    for ( size_t i = 0; i < 5; i++ )
    {
        star::Threading::ptr new_thread(
        new star::Threading( test_case4, "chunk_lock_test" ) );
        threads.push_back( new_thread );
    }

    for ( auto item : threads )
    {
        item->join();
    }

    return 0;
}