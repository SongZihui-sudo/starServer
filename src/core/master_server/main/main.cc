#include "../master_server.h"

#include <star.h>
#include <string>
#include <functional>

/* 服务器对象 */
star::master_server::ptr cs;

star::Logger::ptr global_logger( STAR_NAME( "global_logger" ) );

void run() { cs->wait( cs->respond, cs.get() ); }

/*
    Chunk 服务器
 */
int main()
{
    cs.reset( new star::master_server( "./master_server_settings.json" ) );

    INFO_STD_STREAM_LOG( global_logger ) << std::to_string( getTime() ) << " <----> "
                                         << "Server initialization completed."
                                         << "%n%0";

    /* 新建一个线程，等待连接 */
    run();

    return 0;
}