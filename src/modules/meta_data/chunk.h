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

    chunk( std::string name, std::string path, size_t index );
    ~chunk() = default;

public:
    /*
        打开块
     */
    bool open( levelDB::ptr db_ptr );

    /*
        写块
     */
    bool write( file_operation flag, std::string buffer );

    /*
        读块
     */
    bool read( file_operation flag, std::string& buffer );

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
        移动路径
     */
    void move_path( std::string new_path ) { this->m_path = new_path; }

    /* 删块数据 */
    bool del( file_operation flag );

    /* 获取序号 */
    size_t get_index() { return this->index; }

public:
    std::string addr;
    int16_t port;

private:
    io_lock::ptr m_lock; /* 锁 */
    levelDB::ptr m_db;   /* 指向数据库的指针 */
    std::string m_name;  /* 文件名 */
    std::string m_path;  /* 文件路径 */
    size_t index;        /* 序号 */
    std::string buffer;  /* 缓冲区 */
    int32_t m_size;      /* 块大小 */
    bool open_flag = false;
};
}

#endif