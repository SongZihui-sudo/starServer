#include "../master_server.h"
/* 服务器对象 */
star::master_server::ptr cs;

void run() { cs->wait(); }

/*
    Chunk 服务器
 */
int main()
{
    cs.reset( new star::master_server() );
    /* 新建一个线程，等待连接 */
    
    run();

    return 0;
}