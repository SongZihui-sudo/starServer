#ifndef MASTER_SERVER_H
#define MASTER_SERVER_H

#include "core/tcpServer/tcpserver.h"
#include "modules/db/database.h"
#include "modules/log/log.h"
#include "modules/meta_data/chunk.h"
#include "modules/protocol/protocol.h"
#include "modules/socket/socket.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <map>
#include <stack>
#include <star.h>
#include <string>
#include <vector>

namespace star
{
static Logger::ptr g_logger( STAR_NAME( "master_logger" ) );
/*
    master server 服务器
*/
class master_server : public tcpserver
{
public:
    /*
        chunk server 信息
     */
    struct chunk_server_info
    {
        std::string addr;               /* ip 地址 */
        std::int64_t port;              /* 端口 */
        int64_t current_operation_time; /* 最近一次操作的时间 */
        bool is_need_sync = false;              /* 是否需要同步 */

        /* 生成一个关于 chunk server 地址的键值 */
        std::string join() { return this->addr + ":" + S( this->port ); }

        bool operator==( chunk_server_info other )
        {
            if ( this->addr == other.addr && this->port == other.port )
            {
                return true;
            }
            return false;
        }

        static std::string join( std::string addr, std::int64_t port )
        {
            return addr + ":" + S( port );
        }
    };

public:
    typedef std::shared_ptr< master_server > ptr;

    master_server( std::filesystem::path settings_path );
    virtual ~master_server() = default;

public:
    /* 心跳机制，定时向 chunk server 发消息，判断其是否在线 */
    static void heart_beat();

    /* 向 chunk server 发消息，判断其是否存在 */
    static void check_chunk_server();

    /* 连接chunk server失败的回调函数 */
    static void chunk_server_connect_fail( int index );

    /* 查找指定文件的元数据信息 */
    virtual bool find_file_meta_data( std::vector< std::string >& res, std::string f_name, std::string f_path );

    /* 使用副本替换失联 chunk server 上的 chunk */
    void replace_unconnect_chunk();

    /* 与 chunk server 连接上后，通过副本同步 chunk */
    static void sync_chunk( chunk_server_info cur_server );

public:
    /* 用户登录认证 */
    bool login( std::string user_name, std::string pwd );

    /* 用户注册 */
    bool regist( std::string user_name, std::string pwd );

    /* 响应连接 */
    static void respond();

protected:
    /* 响应函数 */
    static void deal_with_101( std::vector< void* > args ); /* 客户端请求指定文件的元数据 */

    static void deal_with_104( std::vector< void* > args ); /* 客户端上传文件 */

    static void deal_with_117( std::vector< void* > args ); /* 修改文件路径 */

    static void deal_with_118( std::vector< void* > args ); /* 用户认证 */

    static void deal_with_119( std::vector< void* > args ); /* 注册用户认证信息 */

    static void deal_with_126( std::vector< void* > args ); /* 客户端请求已经上传的文件元数据 包含 文件名，路径 */

    /* 消息映射函数表 */
    std::map< int32_t, std::function< void( std::vector< void* > ) > > message_funcs
    = { { 101, std::function< void( std::vector< void* > ) >( deal_with_101 ) },
        { 104, std::function< void( std::vector< void* > ) >( deal_with_104 ) },
        { 117, std::function< void( std::vector< void* > ) >( deal_with_117 ) },
        { 118, std::function< void( std::vector< void* > ) >( deal_with_118 ) },
        { 119, std::function< void( std::vector< void* > ) >( deal_with_119 ) },
        { 126, std::function< void( std::vector< void* > ) >( deal_with_126 ) } };

    /* 加密用户密码 */
    static std::string encrypt_pwd( std::string pwd );

    /* 打印一个字符画 logo */
    void print_logo()
    {
        DOTKNOW_STD_STREAM_LOG( g_logger )
        << "\n  ____    _                    _____       \n"
        << " / ___|  | |_    __ _   _ __  |  ___|  ___ \n"
        << " \\___ \\  | __|  / _` | | '__| | |_    / __|\n"
        << "  ___) | | |_  | (_| | | |    |  _|   \\__ \\\n"
        << " |____/   \\__|  \\__,_| |_|    |_|     |___/\n"
        << "                            ----> Master Server Exploded version" << Logger::endl();
    }

private:
    static size_t max_chunk_size; /* chunk 的最大大小  */
    bool is_login = false;
    static size_t copys;                    /* 副本个数 */
    static levelDBList::ptr file_name_list; /* 文件名列表 */
    static levelDBList::ptr file_path_list; /* 文件路径列表 */
};
}

#endif