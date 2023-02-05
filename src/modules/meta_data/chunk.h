#ifndef CHUNK_H
#define CHUNK_H

#include "modules/consistency/io_lock/io_lock.h"
#include "modules/db/database.h"
#include "modules/log/log.h"
#include "modules/meta_data/meta_data.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace star
{
class chunk
{
public:
    typedef std::shared_ptr< chunk > ptr;
    chunk( std::string file_url, size_t index );
    chunk( std::string name, std::string path, size_t index );
    ~chunk() = default;

public:
    /*
        打开块
     */
    bool open( file_operation flag, levelDB::ptr db_ptr );

    /*
        写块
     */
    bool write( std::string buffer );

    /*
        读块
     */
    bool read( std::string& buffer );

    /*
        关闭
     */
    void close();

    /*
        大小
     */
    int32_t size() { return this->m_size; };

    /*
        是否被锁
     */
    bool is_lock() { return this->m_lock->is_lock(); }

    /*
        记录元数据
     */
    bool record_meta_data();

    /*
        读元数据
     */
    bool read_meta_data( std::vector< std::string >& res );

    /*
        删元数据
     */
    bool del_meta_data();

    /*
        块拷贝
     */
    bool copy( chunk::ptr other );

    /*
        删块数据
    */
    bool del();

    /*
        获取序号
    */
    size_t get_index() { return this->index; }

    /*
        生成文件资源的 key 值
    */
    static std::string join( std::string file_name, std::string file_path )
    {
        return file_name + "::" + file_path;
    }

    /*
        获取名
    */
    std::string get_name() { return this->m_name; }

    /*
        获取路径
    */
    std::string get_path() { return this->m_path; }

    /*
        获取 url
     */
    std::string get_url() { return this->m_url; }

public:
    std::string addr;
    int16_t port;

private:
    file_operation m_operation_flag; /* 操作标志 */
    std::string m_name;              /* 文件名 */
    std::string m_path;              /* 路径 */
    io_lock::ptr m_lock;             /* 锁 */
    levelDB::ptr m_db;               /* 指向数据库的指针 */
    std::string m_url;               /* url */
    size_t index;                    /* 序号 */
    std::string buffer;              /* 缓冲区 */
    int32_t m_size;                  /* 块大小 */
    bool open_flag = false;
};
}

#endif