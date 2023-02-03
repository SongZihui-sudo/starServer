#include "chunk.h"
#include "core/master_server/master_server.h"
#include "modules/consistency/io_lock/io_lock.h"
#include "modules/log/log.h"
#include "modules/meta_data/meta_data.h"
#include <string>

namespace star
{
chunk::chunk( std::string name, std::string path, size_t index )
{
    this->m_name = name;
    this->m_path = path;
    this->index  = index;
    this->m_lock.reset( new io_lock() );
}

bool chunk::open( levelDB::ptr db_ptr )
{
    if ( !db_ptr )
    {
        return false;
    }

    this->m_db = db_ptr;

    std::string temp = "";

    this->open_flag = true;

    return true;
}

bool chunk::write( file_operation flag, std::string buffer )
{
    if ( !open_flag )
    {
        return false;
    }

    this->m_lock->lock_write( flag ); /* 上写锁 */

    /* 拼一下键值 */
    std::string key = levelDB::joinkey( { this->m_name, this->m_path, S( index ) } );

    bool write_flag = this->m_db->Put( key, buffer );
    this->m_size += buffer.size();

    this->m_lock->release_write(); /* 解开写锁 */

    return write_flag;
}

bool chunk::read( file_operation flag, std::string& buffer )
{
    if ( !open_flag )
    {
        return false;
    }

    this->m_lock->lock_read( flag ); /* 上读锁 */

    /* 拼一下键值 */
    std::string key = levelDB::joinkey( { this->m_name, this->m_path, S( index ) } );

    bool write_flag = this->m_db->Get( key, buffer );

    this->m_lock->release_read(); /* 解开读锁 */

    return write_flag;
}

void chunk::close()
{
    // this->m_db = nullptr;
    this->open_flag = false;
}

bool chunk::record_meta_data()
{
    if ( !open_flag )
    {
        ERROR_STD_STREAM_LOG( g_logger ) << "Error, what: "
                                         << "not open chunk!"
                                         << Logger::endl();
        return false;
    }

    this->m_lock->lock_write( file_operation::write ); /* 上写锁 */

    std::string key = levelDB::joinkey( { this->m_name, this->m_path, S( index ), "addr" } );
    bool write_flag = this->m_db->Put( key, this->addr );

    if ( !write_flag )
    {
        this->m_lock->lock_write( file_operation::write ); /* 上写锁 */
        return false;
    }

    key        = levelDB::joinkey( { this->m_name, this->m_path, S( index ), "port" } );
    write_flag = this->m_db->Put( key, S( this->port ) );

    key        = levelDB::joinkey( { this->m_name, this->m_path, S( index ), "index" } );
    write_flag = this->m_db->Put( key, S( this->index ) );

    this->m_lock->release_write(); /* 解开写锁 */

    return write_flag;
}

bool chunk::read_meta_data( std::vector< std::string >& res )
{
    if ( !open_flag )
    {
        return false;
    }
    this->m_lock->lock_read( file_operation::read ); /* 上读锁 */

    std::string key = levelDB::joinkey( { this->m_name, this->m_path, S( index ), "addr" } );
    std::string temp;
    bool read_flag = this->m_db->Get( key, temp );
    if ( !read_flag )
    {
        this->m_lock->release_read(); /* 解开读锁 */
        return false;
    }
    res.push_back( temp );

    key       = levelDB::joinkey( { this->m_name, this->m_path, S( index ), "port" } );
    read_flag = this->m_db->Get( key, temp );
    if ( !read_flag )
    {
        this->m_lock->release_read(); /* 解开读锁 */
        return false;
    }
    res.push_back( temp );

    key       = levelDB::joinkey( { this->m_name, this->m_path, S( index ), "index" } );
    read_flag = this->m_db->Get( key, temp );
    if ( !read_flag )
    {
        this->m_lock->release_read(); /* 解开读锁 */
        return false;
    }
    res.push_back( temp );

    this->m_lock->release_read(); /* 解开读锁 */

    return read_flag;
}

bool chunk::copy( chunk::ptr other )
{
    this->m_lock->lock_write( file_operation::write );
    this->m_db   = other->m_db;
    this->m_name = other->m_name;
    this->m_path = other->m_path;
    this->m_size = other->m_size;
    this->m_lock->release_write();
    return true;
}

bool chunk::del_meta_data()
{
    if ( !open_flag )
    {
        return false;
    }
    this->m_lock->lock_read( file_operation::read ); /* 上读锁 */
    std::string key = levelDB::joinkey( { this->m_name, this->m_path, S( index ), "addr" } );
    bool flag       = this->m_db->Delete( key );
    if ( !flag )
    {
        this->m_lock->lock_read( file_operation::read ); /* 上读锁 */
        return false;
    }
    key  = levelDB::joinkey( { this->m_name, this->m_path, S( index ), "port" } );
    flag = this->m_db->Delete( key );
    if ( !flag )
    {
        this->m_lock->lock_read( file_operation::read ); /* 上读锁 */
        return false;
    }

    key  = levelDB::joinkey( { this->m_name, this->m_path, S( index ), "index" } );
    flag = this->m_db->Delete( key );
    if ( !flag )
    {
        this->m_lock->lock_read( file_operation::read ); /* 上读锁 */
        return false;
    }

    return true;
}

bool chunk::del( file_operation flag )
{
    if ( !open_flag )
    {
        return false;
    }

    this->m_lock->lock_write( flag );
    std::string key = levelDB::joinkey( { this->m_name, this->m_path, S( index ) } );
    bool del_flag   = this->m_db->Delete( key );
    this->m_lock->release_write();
    return del_flag;
}

}