#include "../chunk_server.h"
/* 服务器对象 */
star::chunk_server::ptr cs;

void run() { cs->wait( cs->respond, cs.get() ); }

star::Logger::ptr global_logger( STAR_NAME( "global_logger" ) );

/*
    Chunk 服务器
 */
int main()
{
    cs.reset( ( new star::chunk_server( "./chunk_server_settins.json" ) ) );

    /* 新建一个线程，等待连接 */
    star::Threading::ptr new_thread( new star::Threading( run, "Server Thread" ) );

    INFO_STD_STREAM_LOG( global_logger ) << std::to_string( getTime() ) << " <----> "
                                         << "Server initialization completed."
                                         << "%n%0";

    new_thread->join();

    return 0;
}