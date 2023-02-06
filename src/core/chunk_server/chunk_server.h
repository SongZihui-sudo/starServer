#ifndef CHUNK_SERVER_H
#define CHUNK_SERVER_H

#include "core/tcpServer/tcpserver.h"
#include "modules/log/log.h"
#include "modules/meta_data/meta_data.h"
#include "modules/protocol/protocol.h"
#include "modules/socket/address.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <stack>
#include <star.h>
#include <string>
#include <vector>

namespace star
{
static Logger::ptr g_logger( STAR_NAME( "master_logger" ) );

/* Chunk 服务器 */
class chunk_server : public tcpserver
{
protected:
    struct master_server_info
    {
        std::string addr;
        int64_t port;
    };

public:
    typedef std::shared_ptr< chunk_server > ptr;

    chunk_server( std::filesystem::path settins_path );
    ~chunk_server() = default;

public:
    /* 响应 */
    static void respond();

protected:
    /* 响应函数 */
    static void deal_with_108( std::vector< void* > args ); /* 接受 master server 发来的 chunk 数据 */

    static void deal_with_122( std::vector< void* > args ); /* 修改块的路径 */

    static void deal_with_113( std::vector< void* > args ); /* 删除块的数据 */

    static void deal_with_110( std::vector< void* > args ); /* 给客户端发送块数据 */

    static void deal_with_137(std::vector< void* > args);   /* 块重命名 */

    /* 响应函数映射表 */
    std::map< int32_t, std::function< void( std::vector< void* > ) > > message_funcs
    = { { 108, std::function< void( std::vector< void* > ) >( deal_with_108 ) },
        { 122, std::function< void( std::vector< void* > ) >( deal_with_122 ) },
        { 113, std::function< void( std::vector< void* > ) >( deal_with_113 ) },
        { 110, std::function< void( std::vector< void* > ) >( deal_with_110 ) } };

    /* 打印一个字符画 */
    void print_logo()
    {
        DOTKNOW_STD_STREAM_LOG( g_logger )
        << "\n  ____    _                    _____       \n"
        << " / ___|  | |_    __ _   _ __  |  ___|  ___ \n"
        << " \\___ \\  | __|  / _` | | '__| | |_    / __|\n"
        << "  ___) | | |_  | (_| | | |    |  _|   \\__ \\\n"
        << " |____/   \\__|  \\__,_| |_|    |_|     |___/\n"
        << "                            ----> Chunk Server Exploded version"
        << Logger::endl();
    }

private:
    master_server_info m_master; /* master server 信息 */
    size_t max_chunk_size;       /* chunk 的最大大小  */
    std::map< std::string, std::vector< chunk_meta_data > > chunk_list; /* chunk 列表 */
    std::stack< protocol::Protocol_Struct > updo_ss;
};
}

#endif