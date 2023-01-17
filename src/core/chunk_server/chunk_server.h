#ifndef CHUNK_SERVER_H
#define CHUNK_SERVER_H

#include <list>
#include <memory>
#include <star.h>
#include <string>
#include <vector>

namespace star
{
/* Chunk 服务器 */
class chunk_server
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

    /*
        元数据
     */
    struct chunk_meta_data
    {
        std::string f_name; /* 文件名 */
        size_t index;       /* 索引 */
        const char* data;   /* 数据 */
        std::string f_path; /* 文件路径 */
        size_t chunk_size;  /* chunk 大小 */
        std::string from;   /* 所在的 chunk server */
        int port;           /* chunk server 端口 */

        chunk_meta_data( std::string f_name,
                         size_t index,
                         const char* data,
                         std::string path,
                         size_t chunk_size,
                         std::string from,
                         int port )
        {
            this->f_name     = f_name;
            this->index      = index;
            this->data       = data;
            this->f_path     = path;
            this->chunk_size = chunk_size;
            this->from       = from;
            this->port       = port;
        }

        chunk_meta_data() {}
    };

public:
    typedef std::shared_ptr<chunk_server> ptr;

    chunk_server();
    ~chunk_server() = default;

public:
    /* 等待连接 */
    void wait();

    /* 服务器关闭 */
    void close();

    /* 读一个块 */
    virtual void get_chunk( chunk_meta_data c_chunk, const char*& data );

    /* 写一个块 */
    virtual void write_chunk( chunk_meta_data c_chunk );

    /* 删一个块 */
    virtual void delete_chunk( chunk_meta_data c_chunk );

    /* 导出数据库中的元数据 */
    virtual void get_all_meta_data( std::vector< chunk_meta_data >& arr );

private:
    /* 响应 */
    static void respond();

private:
    Status m_status;          /* 服务器状态 */
    config::ptr m_settings;   /* 服务器设置 */
    levelDB::ptr m_db;        /* 服务器数据库 */
    std::string m_name;       /* 服务器名 */
    Logger::ptr m_logger;     /* 日志器 */
    MSocket::ptr m_sock;      /* socket */
    protocol::ptr m_protocol; /* 协议序列化 */
    size_t max_chunk_size;    /* chunk 的最大大小  */
};
}

#endif