#ifndef MASTER_SERVER_H
#define MASTER_SERVER_H

#include "core/tcpServer/tcpserver.h"
#include "modules/log/log.h"
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
/*
    master server 服务器
*/
class master_server : public tcpserver
{
protected:
    /*
        chunk server 信息
     */
    struct chunk_server_info
    {
        size_t space;      /* 存储空间 */
        std::string addr;  /* ip 地址 */
        std::int64_t port; /* 端口 */
    };

public:
    typedef std::shared_ptr< master_server > ptr;

    master_server( std::filesystem::path settings_path );
    virtual ~master_server() = default;

public:
    /* 向 chunk server 询问 块元数据 */
    virtual void ask_chunk_meta_data();

    /* 查找指定文件的元数据信息 */
    virtual bool find_file_meta_data( file_meta_data& data, std::string f_name, std::string f_path );

    /* 查找 chunk 的元数据信息 */
    virtual bool find_chunk_meta_data( std::string f_name, int index, chunk_meta_data& data );

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

    static void deal_with_102( std::vector< void* > args ); /* 更新文件元数据表 */

    static void deal_with_104( std::vector< void* > args ); /* 客户端上传文件 */

    static void deal_with_117( std::vector< void* > args ); /* 修改文件路径 */

    static void deal_with_118( std::vector< void* > args ); /* 用户认证 */

    static void deal_with_119( std::vector< void* > args ); /* 注册用户认证信息 */

    static void deal_with_126( std::vector< void* > args ); /* 客户端请求已经上传的文件元数据 包含 文件名，路径 */

    /* 消息映射函数表 */
    std::map< int32_t, std::function< void( std::vector< void* > ) > > message_funcs
    = { { 101, std::function< void( std::vector< void* > ) >( deal_with_101 ) },
        { 102, std::function< void( std::vector< void* > ) >( deal_with_102 ) },
        { 104, std::function< void( std::vector< void* > ) >( deal_with_104 ) },
        { 117, std::function< void( std::vector< void* > ) >( deal_with_117 ) },
        { 119, std::function< void( std::vector< void* > ) >( deal_with_118 ) },
        { 126, std::function< void( std::vector< void* > ) >( deal_with_126 ) } };

    /* 加密用户密码 */
    static std::string encrypt_pwd( std::string pwd );

    /* 打印一个字符画 */
    void print_logo()
    {
        DOTKNOW_STD_STREAM_LOG( this->m_logger )
        << "\n  ____    _                    _____       \n"
        << " / ___|  | |_    __ _   _ __  |  ___|  ___ \n"
        << " \\___ \\  | __|  / _` | | '__| | |_    / __|\n"
        << "  ___) | | |_  | (_| | | |    |  _|   \\__ \\\n"
        << " |____/   \\__|  \\__,_| |_|    |_|     |___/\n"
        << "                            ----> Master Server Exploded version"
        << "%n%0";
    }

private:
    size_t max_chunk_size;                              /* chunk 的最大大小  */
    std::vector< chunk_server_info > chunk_server_list; /* chunk server 信息 */
    bool is_login = false;
    size_t copys; /* 副本个数 */
};
}

#endif