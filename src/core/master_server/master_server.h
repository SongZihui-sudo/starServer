#ifndef MASTER_SERVER_H
#define MASTER_SERVER_H

#include "core/tcpServer/tcpserver.h"
#include "modules/log/log.h"

#include <cstdint>
#include <star.h>
#include <map>
#include <string>
#include <vector>
#include <filesystem>

namespace star
{
/*
    master server 服务器
*/
class master_server : public tcpserver
{
protected:
    struct chunk_server_info
    {
        std::string addr;
        std::int64_t port;
    };

public:
    typedef std::shared_ptr< master_server > ptr;

    master_server(std::filesystem::path settings_path);
    virtual ~master_server() = default;

public:
    /* 查找指定文件的元数据信息 */
    virtual bool find_file_meta_data( file_meta_data& data, std::string f_name );

    /* 查找 chunk 的元数据信息 */
    virtual bool find_chunk_meta_data( std::string f_name, int index, chunk_meta_data& data );

    /* 划分chunk */
    virtual void Split_file( std::string f_name,
                             const char* f_data,
                             std::string path,
                             std::map< std::string, file_meta_data >& meta_data_tab );

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
    size_t max_chunk_size;                                 /* chunk 的最大大小  */
    std::map< std::string, file_meta_data > meta_data_tab; /* 元数据表 */
    std::vector< chunk_server_info > chunk_server_list;    /* chunk server 信息 */
};
}

#endif