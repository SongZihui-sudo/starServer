#ifndef MASTER_SERVER_H
#define MASTER_SERVER_H

#include "modules/log/log.h"
#include <cstdint>
#include <star.h>

#include <map>
#include <vector>

namespace star
{
/*
    master server 服务器
*/
class master_server
{
protected:
    /*
        服务器状态
     */
    enum Status
    {
        Normal        = 0,
        NOT_FOUND     = 1,
        BREAKDOWN     = 2,
        OK            = 3,
        ERROR         = 4,
        COMMAND_ERROR = 5,
        PARSING_ERROR = 6,
        INIT          = 7
    };

    struct chunk_server_info
    {
        std::string addr;
        std::int64_t port;
    };

public:
    typedef std::shared_ptr< master_server > ptr;

    master_server();
    ~master_server() = default;

public:
    /* 等待连接 */
    virtual void wait();

    /* 服务器关闭 */
    virtual void close();

    /* 查找指定文件的元数据信息 */
    virtual bool find_file_meta_data( file_meta_data& data, std::string f_name );

    /* 查找 chunk 的元数据信息 */
    virtual bool find_chunk_meta_data( std::string f_name, int index, chunk_meta_data& data );

    /* 划分chunk */
    virtual void Split_file( std::string f_name,
                             const char* f_data,
                             std::string path,
                             std::map< std::string, file_meta_data >& meta_data_tab );

    /* 获取服务器名 */
    std::string get_name() { return this->m_name; }

    /* 获取服务器状态 */
    Status get_status() { return this->m_status; }

public:
    /* 用户登录认证 */
    bool login( std::string user_name, std::string pwd );

    /* 用户注册 */
    bool regist( std::string user_name, std::string pwd );

protected:
    /* 响应连接 */
    static void respond();

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
    Status m_status;                                       /* 服务器状态 */
    config::ptr m_settings;                                /* 服务器设置 */
    std::string m_name;                                    /* 服务器名 */
    Logger::ptr m_logger;                                  /* 日志器 */
    MSocket::ptr m_sock;                                   /* socket */
    protocol::ptr m_protocol;                              /* 协议序列化 */
    size_t max_chunk_size;                                 /* chunk 的最大大小  */
    std::map< std::string, file_meta_data > meta_data_tab; /* 元数据表 */
    levelDB::ptr m_db;                                     /* 服务器数据库 */
    std::vector< chunk_server_info > chunk_server_list;    /* chunk server 信息 */
};
}

#endif