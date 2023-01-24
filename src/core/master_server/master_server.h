#ifndef MASTER_SERVER_H
#define MASTER_SERVER_H

#include "core/tcpServer/tcpserver.h"
#include "modules/log/log.h"
#include "modules/protocol/protocol.h"
#include "modules/socket/socket.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
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
    std::stack< protocol::Protocol_Struct > updo_ss; /* 上一步，进一步可以拓展为快照 */
    bool is_login = false;
    size_t copys;   /* 副本个数 */
};
}

#endif