#ifndef MASTER_SERVER_H
#define MASTER_SERVER_H

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
    /*
        块元数据
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

    /* 文件元数据 */
    struct file_meta_data
    {
        std::string f_name;                        /* 文件名 */
        std::vector< chunk_meta_data > chunk_list; /* 文件列表 */
        size_t f_size;                             /* 文件大小 */
        size_t num_chunk;                          /* 文件块数 */

        protocol::Protocol_Struct toProrocol()
        {
            protocol::Protocol_Struct ret;

            /* 把元数据转换成为协议结构体 */
            ret.file_name = this->f_name;
            ret.file_size = this->f_size;
            ret.bit       = 1;
            ret.path.push_back( this->chunk_list[0].f_path );
            for ( size_t i = 0; i < this->chunk_list.size(); i++ )
            {
                ret.data.push_back( this->chunk_list[i].from );
                ret.customize.push_back( ( int* )this->chunk_list[i].index );
                ret.customize.push_back( ( std::string* )this->chunk_list[i].from.c_str() );
            }

            return ret;
        }
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