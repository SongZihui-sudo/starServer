#include "file.h"
#include "core/master_server/master_server.h"
#include "modules/consistency/io_lock/io_lock.h"
#include "modules/db/database.h"
#include "modules/log/log.h"
#include "modules/meta_data/chunk.h"
#include "modules/meta_data/meta_data.h"
#include <asm-generic/errno.h>
#include <cstddef>
#include <string>
#include <vector>

namespace star
{

file::file( std::string file_name, std::string file_path, int32_t default_chunk_size )
{
    this->default_chunk_size = default_chunk_size;
    this->m_name             = file_name;
    this->m_path             = file_path;
}

bool file::open( levelDB::ptr db_ptr )
{
    if ( !db_ptr )
    {
        return false;
    }

    this->m_db      = db_ptr;
    this->open_flag = true;

    this->set_chunks_num();
    for ( size_t i = 0; i < this->m_chunks_num; i++ )
    {
        chunk::ptr cur_chunk( new chunk( this->m_name, this->m_path, i ) );
        this->chunks.push_back( cur_chunk );
    }

    this->current_operation_time = getTime();

    return true;
}

bool file::write( file_operation flag, std::string buffer, size_t index )
{
    if ( !open_flag )
    {
        return false;
    }

    if ( index < 0 || index > this->chunks.size() )
    {
        return false;
    }
    bool ret_flag;
    this->chunks[index]->open( m_db );
    ret_flag = this->chunks[index]->write( flag, buffer );
    this->chunks[index]->close();
    this->current_operation_time = getTime();

    return ret_flag;
}

bool file::read( file_operation flag, std::string& buffer, size_t index )
{
    if ( !open_flag )
    {
        return false;
    }

    if ( index < 0 || index > this->chunks.size() )
    {
        return false;
    }
    bool ret_flag;
    this->chunks[index]->open( m_db );
    ret_flag = this->chunks[index]->write( flag, buffer );
    this->chunks[index]->close();
    this->current_operation_time = getTime();

    return ret_flag;
}

void file::close()
{
    // this->m_db      = nullptr;
    this->open_flag              = false;
    this->current_operation_time = getTime();
}

bool file::append( file_operation flag, std::string buffer )
{
    if ( !open_flag )
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "Error, what: "
                                         << "not open file!" << Logger::endl();
        return false;
    }

    bool ret_flag;

    DEBUG_STD_STREAM_LOG( g_logger ) << "file.cc 114"
                                     << "append" << Logger::endl();
    chunk::ptr new_chunk( new chunk( this->m_name, this->m_path, this->chunks.size() ) );
    new_chunk->open( m_db );
    ret_flag = new_chunk->write( file_operation::write, buffer );
    this->chunks.push_back( new_chunk );
    new_chunk->close();

    this->current_operation_time = getTime();
    this->m_chunks_num++;
    this->m_db->Put( this->join(), S( this->m_chunks_num ) );

    return ret_flag;
}

bool file::del( file_operation flag, size_t index )
{
    if ( !open_flag )
    {
        return false;
    }

    if ( index < 0 || index > this->chunks.size() )
    {
        return false;
    }
    bool is_lock = this->chunks[index]->is_lock();

    /* 被锁 */
    while ( is_lock )
    {
    }
    /* 等待解锁后，删这个块 */
    this->chunks[index]->open( this->m_db );
    this->chunks[index]->del( file_operation::write );
    this->chunks[index]->close();
    this->current_operation_time = getTime();
    this->m_chunks_num--;
    this->m_db->Put( this->join(), S( this->m_chunks_num ) );

    return true;
}

/* 路径修改 */
bool file::move( std::string new_path )
{
    /* 临时文件 */
    file::ptr new_file( new file( this->m_name, new_path, this->default_chunk_size ) );
    /* 拷贝过去 */
    bool flag = new_file->copy( file::ptr( this ) );
    if ( !flag )
    {
        return false;
    }
    /* 删除原来路径 */
    for ( size_t i = 0; i < this->chunks.size(); i++ )
    {
        this->del( file_operation::write, i );
    }
    this->m_path = new_path;
    /* 再拷贝回来 */
    this->copy( new_file );
    /* 重写元数据 */
    for ( size_t i = 0; i < this->chunks.size(); i++ )
    {
        std::vector< std::string > res;
        /* 读原来的元数据 */
        this->read_chunk_meta_data( i, res );
        this->record_chunk_meta_data( i, res[0], std::stoi( res[1] ) );
        this->del_chunk_meta_data( i ); /* 删原来的元数据 */
        std::string data;               /* 读块数据 */
        this->read( file_operation::read, data, i );
        if ( !data.empty() )
        {
            /* 重新写数据 */
            this->chunks[i]->move_path( new_path );
            this->write( file_operation::write, data, i );
            this->del( file_operation::write, i ); /* 删原来的块 */
        }
    }
    this->current_operation_time = getTime();

    return true;
}

/* 文件拷贝 */
bool file::copy( file::ptr other )
{
    std::vector< chunk::ptr > chunk_list = other->get_chunk_list();
    for ( size_t i = 0; chunk_list.size(); i++ )
    {
        chunk::ptr temp;
        bool flag = temp->copy( chunk_list[i] );
        if ( !flag )
        {
            return false;
        }
        this->chunks.push_back( temp );
    }
    this->current_operation_time = getTime();

    return true;
}

bool file::record_chunk_meta_data( int index, std::string addr, int16_t port )
{
    if ( index < 0 || index > this->chunks.size() )
    {
        return false;
    }

    this->chunks[index]->addr = addr;
    this->chunks[index]->port = port;
    this->chunks[index]->open( this->m_db );
    bool write_flag = this->chunks[index]->record_meta_data();
    this->chunks[index]->close();
    this->current_operation_time = getTime();
    return true;
}

bool file::read_chunk_meta_data( int index, std::vector< std::string >& res )
{
    if ( index < 0 || index > this->chunks.size() )
    {
        return false;
    }

    this->chunks[index]->open( this->m_db );
    bool read_flag = this->chunks[index]->read_meta_data( res );
    this->chunks[index]->close();
    this->current_operation_time = getTime();
    return read_flag;
}

bool file::del_chunk_meta_data( int index )
{
    if ( index < 0 || index > this->chunks.size() )
    {
        return false;
    }

    this->chunks[index]->open( this->m_db );
    this->chunks[index]->del_meta_data();
    this->chunks[index]->close();
    this->current_operation_time = getTime();
    return true;
}

bool file::append_meta_data( std::string addr, int16_t port )
{
    if ( !open_flag )
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "Error, what: "
                                         << "not open file!" << Logger::endl();
        return false;
    }
    chunk::ptr new_chunk( new chunk( this->m_name, this->m_path, this->chunks.size() ) );
    new_chunk->addr = addr;
    new_chunk->port = port;
    new_chunk->open( this->m_db );
    bool write_flag = new_chunk->record_meta_data();
    new_chunk->close();
    this->chunks.push_back( new_chunk );
    this->current_operation_time = getTime();
    this->m_chunks_num++;
    this->m_db->Put( this->join(), S( this->m_chunks_num ) );
    return write_flag;
}

void file::set_chunks_num()
{
    std::string value;
    if ( !this->m_db->Get( this->join(), value ) )
    {
        this->m_chunks_num = 0;
    }
    else
    {
        this->m_chunks_num = std::stoi( value );
    }
    DEBUG_STD_STREAM_LOG( g_logger )
    << "Get a chunks, size: " << S( this->m_chunks_num ) << Logger::endl();
}

}
