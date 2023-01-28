#ifndef FILE_H
#define FILE_H

#include "meta_data.h"
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

    file( std::string file_name, std::string file_path, int32_t default_chunk_size );
    ~file() = default;

public:
    /*
        打开文件
     */
    bool open( levelDB::ptr db_ptr );

    /*
        写文件
     */
    bool write( file_operation flag, std::string buffer, size_t index );

    /*
        读文件
     */
    bool read( file_operation flag, std::string& buffer, size_t index );

    /*
        追加数据
     */
    bool append( file_operation flag, std::string buffer );

    /*
        删块
     */
    bool del( file_operation flag, size_t index );

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
        if ( this->chunks.empty() )
        {
            this->chunks.push_back(
            chunk::ptr( new chunk( this->m_name, this->m_path, this->m_chunks_num ) ) );
        }
        return this->chunks.back();
    }

    /* 获取其第一个块 */
    chunk::ptr begin()
    {
        if ( this->chunks.empty() )
        {
            this->chunks.push_back(
            chunk::ptr( new chunk( this->m_name, this->m_path, this->m_chunks_num ) ) );
        }
        return this->chunks.front();
    }

    /* 记录块的元数据 */
    bool record_chunk_meta_data( int index, std::string addr, int16_t port )
    {
        if ( index < 0 || index > this->chunks.size() )
        {
            return false;
        }
        this->chunks[index]->addr = addr;
        this->chunks[index]->port = port;
        this->chunks[index]->open( this->m_db );
        bool write_flag           = this->chunks[index]->record_meta_data();
        this->chunks[index]->close();
        return write_flag;
    }

    /* 读块的元数据 */
    bool read_chunk_meta_data( int index, std::vector< std::string >& res )
    {
        if ( index < 0 || index > this->chunks.size() )
        {
            return false;
        }
        this->chunks[index]->open( this->m_db );
        bool read_flag = this->chunks[index]->read_meta_data( res );
        this->chunks[index]->close();
        return read_flag;
    }

    /* 删块的元数据 */
    bool del_chunk_meta_data( int index )
    {
        if ( index < 0 || index > this->chunks.size() )
        {
            return false;
        }
        this->chunks[index]->open( this->m_db );
        this->chunks[index]->del_meta_data();
        this->chunks[index]->close();
        return true;
    }

    /* 获取路径 */
    std::string get_path() { return this->m_path; }

    /* 获取文件名 */
    std::string get_name() { return this->m_name; }

    /* 块数 */
    size_t chunks_num() { return this->chunks.size(); }

    /* 路径修改 */
    bool move( std::string new_path );

    /* 文件拷贝 */
    bool copy( file::ptr other );

    /* 获取chunk链 */
    std::vector< chunk::ptr > get_chunk_list() { return this->chunks; }

private:
    levelDB::ptr m_db;                /* 指向数据库的指针 */
    bool open_flag;                   /* 是否打开 */
    std::vector< chunk::ptr > chunks; /* chunk 信息 */
    std::string m_name;               /* 文件名 */
    std::string m_path;               /* 文件路径 */
    int32_t m_chunks_num;             /* 块数 */
    int32_t default_chunk_size;
};

}

#endif