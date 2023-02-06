#include "file.h"
#include "modules/common/common.h"
#include "modules/consistency/io_lock/io_lock.h"
#include "modules/db/database.h"
#include "modules/log/log.h"
#include <cstddef>
#include <string>
#include <vector>

namespace star
{

static Logger::ptr g_logger( STAR_NAME( "file_logger" ) );

file::file( std::string file_url, int32_t default_chunk_size )
{
    this->default_chunk_size = default_chunk_size;
    this->m_lock.reset( new io_lock() );
    this->m_url         = file_url;
    size_t sub_str_end1 = file_url.find( '|' );
    this->m_name        = file_url.substr( 0, sub_str_end1 );
    size_t sub_str_end2 = file_url.find( '|', sub_str_end1 + 1 );
    this->m_path        = file_url.substr( sub_str_end1 + 1, sub_str_end2 - 1 );
    this->create_time = getTime();
}

file::file( std::string file_name, std::string file_path, int32_t default_chunk_size )
{
    this->m_name             = file_name;
    this->m_path             = file_path;
    this->default_chunk_size = default_chunk_size;
    this->m_url              = levelDB::joinkey( { file_name, file_path } );
    this->m_url.pop_back();
    this->m_lock.reset( new io_lock() );
}

bool file::open( file_operation flag, levelDB::ptr db_ptr )
{
    if ( !db_ptr )
    {
        return false;
    }

    this->m_db      = db_ptr;
    this->open_flag = true;

    this->set_chunks_num();

    this->current_operation_time = getTime();
    this->m_operation_flag       = flag;

    return true;
}

bool file::write( std::string buffer, size_t index )
{
    if ( !open_flag )
    {
        return false;
    }

    if ( index < 0 || index > m_chunks_num )
    {
        return false;
    }
    bool ret_flag;
    chunk::ptr cur_chunk( new chunk( this->m_url, index ) );
    cur_chunk->open( this->m_operation_flag, this->m_db );
    cur_chunk->write( buffer );
    cur_chunk->close();
    this->current_operation_time = getTime();

    return ret_flag;
}

bool file::read( std::string& buffer, size_t index )
{
    if ( !open_flag )
    {
        return false;
    }

    if ( index < 0 || index > m_chunks_num )
    {
        return false;
    }
    bool ret_flag;
    chunk::ptr cur_chunk( new chunk( this->m_url, index ) );
    cur_chunk->open( this->m_operation_flag, m_db );
    ret_flag = cur_chunk->read( buffer );
    cur_chunk->close();
    this->current_operation_time = getTime();

    return ret_flag;
}

void file::close()
{
    this->open_flag              = false;
    this->current_operation_time = getTime();
}

bool file::append( std::string buffer )
{
    if ( !open_flag )
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "Error, what: "
                                         << "not open file!" << Logger::endl();
        return false;
    }

    bool ret_flag;

    chunk::ptr new_chunk( new chunk( this->m_url, this->m_chunks_num ) );
    new_chunk->open( this->m_operation_flag, m_db );
    ret_flag = new_chunk->write( buffer );
    new_chunk->close();

    this->m_chunks_num++;
    BREAK( g_logger );
    this->m_lock->lock_write( file_operation::write );
    this->m_db->Put( this->get_url(), S( this->m_chunks_num ) );
    this->current_operation_time = getTime();
    this->m_lock->release_write();
    BREAK( g_logger );

    return ret_flag;
}

bool file::del( size_t index )
{
    if ( !open_flag )
    {
        return false;
    }

    if ( index < 0 || index > this->m_chunks_num )
    {
        return false;
    }

    chunk::ptr new_chunk( new chunk( this->m_url, index ) );
    new_chunk->open( file_operation::write, this->m_db );
    new_chunk->del();
    new_chunk->close();
    this->current_operation_time = getTime();
    this->m_chunks_num--;
    this->m_db->Put( this->get_url(), S( this->m_chunks_num ) );

    return true;
}

/* 重命名 */
bool file::rename( std::string new_name )
{
    /* 临时文件 */
    file::ptr new_file( new file( this->m_url, this->default_chunk_size ) );
    /* 拷贝过去 */
    bool flag = new_file->copy( file::ptr( this ) );
    if ( !flag )
    {
        return false;
    }
    /* 修改路径 */
    this->m_name = new_name;
    this->m_url  = levelDB::joinkey( { this->m_name, new_name } );
    new_file->open( this->m_operation_flag, this->m_db );
    /* 重写元数据 */
    for ( size_t i = 0; i < this->m_chunks_num; i++ )
    {
        std::vector< std::string > res;
        /* 读原来的元数据 */
        new_file->read_chunk_meta_data( i, res );
        this->append_meta_data( res[0], std::stoi( res[1] ) );
        std::string data; /* 读块数据 */
        new_file->read( data, i );
        if ( !data.empty() )
        {
            this->append( data );
        }
    }
    /* 删除原来路径 */
    for ( size_t i = 0; i < this->m_chunks_num; i++ )
    {
        new_file->del( i );
        new_file->del_chunk_meta_data( i ); /* 删原来的元数据 */
    }

    new_file->close();
    this->current_operation_time = getTime();

    return true;
}

/* 路径修改 */
bool file::move( std::string new_path )
{
    /* 临时文件 */
    file::ptr new_file( new file( this->m_url, this->default_chunk_size ) );
    /* 拷贝过去 */
    bool flag = new_file->copy( file::ptr( this ) );
    if ( !flag )
    {
        return false;
    }
    /* 删除原来路径 */
    for ( size_t i = 0; i < this->m_chunks_num; i++ )
    {
        this->del( i );
        this->del_chunk_meta_data( i ); /* 删原来的元数据 */
    }
    /* 修改路径 */
    this->m_path = new_path;
    this->m_url  = levelDB::joinkey( { this->m_name, new_path } );
    new_file->open( this->m_operation_flag, this->m_db );
    /* 重写元数据 */
    for ( size_t i = 0; i < this->m_chunks_num; i++ )
    {
        std::vector< std::string > res;
        /* 读原来的元数据 */
        new_file->read_chunk_meta_data( i, res );
        this->append_meta_data( res[0], std::stoi( res[1] ) );
        std::string data; /* 读块数据 */
        new_file->read( data, i );
        if ( !data.empty() )
        {
            this->append( data );
        }
    }

    new_file->close();
    this->current_operation_time = getTime();
    return true;
}

/* 文件拷贝 */
bool file::copy( file::ptr other )
{
    this->m_name       = other->m_name;
    this->m_path       = other->m_path;
    this->m_url        = other->m_url;
    this->m_chunks_num = other->m_chunks_num;

    return true;
}

bool file::record_chunk_meta_data( int index, std::string addr, int16_t port )
{
    if ( index < 0 || index > this->m_chunks_num )
    {
        return false;
    }

    chunk::ptr new_chunk( new chunk( this->m_url, index ) );
    new_chunk->addr = addr;
    new_chunk->port = port;
    new_chunk->open( this->m_operation_flag, this->m_db );
    bool write_flag = new_chunk->record_meta_data();
    new_chunk->close();
    this->current_operation_time = getTime();
    return true;
}

bool file::read_chunk_meta_data( int index, std::vector< std::string >& res )
{
    if ( index < 0 || index > this->m_chunks_num )
    {
        return false;
    }

    chunk::ptr new_chunk( new chunk( this->m_url, index ) );
    new_chunk->open( this->m_operation_flag, this->m_db );
    bool read_flag = new_chunk->read_meta_data( res );
    new_chunk->close();
    this->current_operation_time = getTime();
    return read_flag;
}

bool file::del_chunk_meta_data( int index )
{
    if ( index < 0 || index > this->m_chunks_num )
    {
        return false;
    }

    chunk::ptr new_chunk( new chunk( this->m_url, index ) );
    new_chunk->open( this->m_operation_flag, this->m_db );
    new_chunk->del_meta_data();
    new_chunk->close();
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
    chunk::ptr new_chunk( new chunk( this->m_url, this->m_chunks_num ) );
    new_chunk->addr = addr;
    new_chunk->port = port;
    new_chunk->open( this->m_operation_flag, this->m_db );
    bool write_flag = new_chunk->record_meta_data();
    new_chunk->close();
    this->current_operation_time = getTime();
    this->m_chunks_num++;
    this->m_db->Put( this->get_url(), S( this->m_chunks_num ) );
    return write_flag;
}

void file::set_chunks_num()
{
    std::string value;
    if ( !this->m_db->Get( this->m_url, value ) )
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
