#include "../chunk_server.h"
/* 服务器对象 */
star::chunk_server::ptr cs( new star::chunk_server( "chunk_server_settins.json" ) );

void run() { cs->wait( cs->respond, cs.get() ); }

/*
    Chunk 服务器
 */
int main()
{

    /* 新建一个线程，等待连接 */
    star::Threading::ptr new_thread( new star::Threading( run, "Server Thread" ) );

    new_thread->join();

    return 0;
}