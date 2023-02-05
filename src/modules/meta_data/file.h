#ifndef FILE_H
#define FILE_H

#include "meta_data.h"
#include "modules/common/common.h"
#include "modules/consistency/io_lock/io_lock.h"
#include "modules/db/database.h"
#include "modules/meta_data/chunk.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace star
{
/* 文件对象 */
class file
{
public:
    typedef std::shared_ptr< file > ptr;
    file( std::string file_url, int32_t default_chunk_size );
    file( std::string file_name, std::string file_path, int32_t default_chunk_size );
    ~file() = default;

public:
    /*
        打开文件
     */
    bool open( file_operation flag, levelDB::ptr db_ptr );

    /*
        写文件
     */
    bool write( std::string buffer, size_t index );

    /*
        读文件
     */
    bool read( std::string& buffer, size_t index );

    /*
        追加数据
     */
    bool append( std::string buffer );

    /*
        删块
     */
    bool del( size_t index );

    /*
        关闭文件
     */
    void close();

    /*
        文件是否已经打开
    */
    bool is_open() { return open_flag; }

    /* 获取其最后一个块 */
    chunk::ptr end()
    {
        chunk::ptr end_chunk( new chunk( this->m_url, this->m_chunks_num - 1 ) );
        return end_chunk;
    }

    /* 获取其第一个块 */
    chunk::ptr begin()
    {
        chunk::ptr beg_chunk( new chunk( this->m_url, 0 ) );
        return beg_chunk;
    }

    /* 记录块的元数据 */
    bool record_chunk_meta_data( int index, std::string addr, int16_t port );

    /* 读块的元数据 */
    bool read_chunk_meta_data( int index, std::vector< std::string >& res );

    /* 删块的元数据 */
    bool del_chunk_meta_data( int index );

    /* 返回文件的url */
    std::string get_url() { return this->m_url; }

    /* 块数 */
    size_t chunks_num() { return this->m_chunks_num; }

    /* 路径修改 */
    bool move( std::string new_path );

    /* 重命名 */
    bool rename( std::string new_name );

    /* 文件拷贝 */
    bool copy( file::ptr other );

    /* 追加一个空块 */
    void append_chunk() { this->m_chunks_num++; }

    /* 追加一个块的元数据 */
    bool append_meta_data( std::string addr, int16_t port );

    /* 生成文件资源的 key 值 */
    static std::string join( std::string file_name, std::string file_path )
    {
        return file_name + "::" + file_path;
    }

    /* 设置 chunks 的大小 */
    void set_chunks_num( size_t chunks_num ) { this->m_chunks_num = chunks_num; }

    /* 重载 */
    void set_chunks_num();

    /*
        获取名
    */
    std::string get_name() { return this->m_name; }

    /*
        获取路径
    */
    std::string get_path() { return this->m_path; }

    /*
        副本的文件名
    */
    static std::string join_copy_name(std::string file_name, size_t index)
    {
        return "copy-" + S(index) + "-" + file_name;
    }

private:
    int64_t current_operation_time;  /* 最近一次操作的时间戳 */
    int64_t create_time;             /* 创建时间 */

private:
    file_operation m_operation_flag; /* 操作标志 */
    std::string m_name;              /* 文件名 */
    std::string m_path;              /* 文件路径 */    
    levelDB::ptr m_db;               /* 指向数据库的指针 */
    bool open_flag;                  /* 是否打开 */
    std::string m_url;               /* url 索引 */
    int32_t m_chunks_num;            /* 块数 */
    int32_t default_chunk_size;
    io_lock::ptr m_lock; /* 锁 */
};

}

#endif