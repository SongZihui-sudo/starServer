#include "../master_server.h"
#include "modules/thread/thread.h"

#include <functional>
#include <star.h>
#include <string>

/* 服务器对象 */
star::master_server::ptr cs;

star::Logger::ptr global_logger( STAR_NAME( "global_logger" ) );

void run()
{
    cs->bind();
    cs->wait( cs->respond, cs.get() );
}

/*
    Chunk 服务器
 */
int main()
{
    cs.reset( new star::master_server( "./master_server_settings.json" ) );

    /* 注册服务 */
    cs->get_service_manager()->register_service( "heart_beat",
                                                 std::function< void() >( star::master_server::heart_beat ) );
    cs->get_service_manager()->register_service( "free_thread_checker",
                                                 std::function< void() >( star::Scheduler::check_free_thread ) );
    cs->get_service_manager()->start();

    INFO_STD_STREAM_LOG( global_logger )
    << std::to_string( getTime() ) << " <----> "
    << "Server initialization completed." << star::Logger::endl();

    /* 新建一个线程，等待连接 */
    run();

    return 0;
}