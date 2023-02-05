#include "chunk.h"
#include "core/master_server/master_server.h"
#include "modules/consistency/io_lock/io_lock.h"
#include "modules/db/database.h"
#include "modules/log/log.h"
#include <string>

namespace star
{
chunk::chunk( std::string file_url, size_t index )
{
    this->m_url = levelDB::joinkey( { file_url, S( index ) } );
    this->index = index;
    this->m_lock.reset( new io_lock() );
    size_t sub_str_end1 = file_url.find( '|' );
    this->m_name        = file_url.substr( 0, sub_str_end1 );
    size_t sub_str_end2 = file_url.find( '|', sub_str_end1 + 1 );
    this->m_path        = file_url.substr( sub_str_end1 + 1, sub_str_end2 - 1 );
    this->m_path.pop_back();
}

chunk::chunk( std::string name, std::string path, size_t index )
{
    this->m_name         = name;
    this->m_path         = path;
    std::string file_url = this->join( name, path );
    this->m_url          = levelDB::joinkey( { file_url, S( index ) } );
    this->index          = index;
    this->m_lock.reset( new io_lock() );
}

bool chunk::open( file_operation flag, levelDB::ptr db_ptr )
{
    if ( !db_ptr )
    {
        return false;
    }

    this->m_db = db_ptr;

    std::string temp = "";

    this->m_operation_flag = flag;
    this->open_flag        = true;

    return true;
}

bool chunk::write( std::string buffer )
{
    if ( !open_flag )
    {
        return false;
    }

    this->m_lock->lock_write( this->m_operation_flag ); /* 上写锁 */

    bool write_flag = this->m_db->Put( this->m_url, buffer );
    this->m_size += buffer.size();

    this->m_lock->release_write(); /* 解开写锁 */

    return write_flag;
}

bool chunk::read( std::string& buffer )
{
    if ( !open_flag )
    {
        return false;
    }

    this->m_lock->lock_read( this->m_operation_flag ); /* 上读锁 */

    bool write_flag = this->m_db->Get( this->m_url, buffer );

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
        return false;
    }

    this->m_lock->lock_write( this->m_operation_flag ); /* 上写锁 */

    std::string key = levelDB::joinkey( { this->m_url, "addr" } );
    bool write_flag = this->m_db->Put( key, this->addr );

    if ( !write_flag )
    {
        this->m_lock->release_write(); /* 上写锁 */
        return false;
    }

    key        = levelDB::joinkey( { this->m_url, "port" } );
    write_flag = this->m_db->Put( key, S( this->port ) );

    key        = levelDB::joinkey( { this->m_url, "index" } );
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
    this->m_lock->lock_read( this->m_operation_flag ); /* 上读锁 */

    std::string key = levelDB::joinkey( { this->m_url, "addr" } );
    std::string temp;
    bool read_flag = this->m_db->Get( key, temp );
    if ( !read_flag )
    {
        this->m_lock->release_read(); /* 解开读锁 */
        return false;
    }
    this->addr = temp;
    res.push_back( temp );

    key       = levelDB::joinkey( { this->m_url, "port" } );
    read_flag = this->m_db->Get( key, temp );
    if ( !read_flag )
    {
        this->m_lock->release_read(); /* 解开读锁 */
        return false;
    }
    res.push_back( temp );
    this->port = std::stoi( temp );

    key       = levelDB::joinkey( { this->m_url, "index" } );
    read_flag = this->m_db->Get( key, temp );
    if ( !read_flag )
    {
        this->m_lock->release_read(); /* 解开读锁 */
        return false;
    }
    res.push_back( temp );
    this->index = std::stoi( temp );

    this->m_lock->release_read(); /* 解开读锁 */

    return read_flag;
}

bool chunk::copy( chunk::ptr other )
{
    this->m_lock->lock_write( this->m_operation_flag );
    this->m_db   = other->m_db;
    other->m_url = this->m_url;
    other->index = this->index;
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
    this->m_lock->lock_read( this->m_operation_flag ); /* 上读锁 */
    std::string key = levelDB::joinkey( { this->m_url, "addr" } );
    bool flag       = this->m_db->Delete( key );
    if ( !flag )
    {
        this->m_lock->release_read();
        return false;
    }
    key  = levelDB::joinkey( { this->m_url, "port" } );
    flag = this->m_db->Delete( key );
    if ( !flag )
    {
        this->m_lock->release_read();
        return false;
    }

    key  = levelDB::joinkey( { this->m_url, "index" } );
    flag = this->m_db->Delete( key );
    if ( !flag )
    {
        this->m_lock->release_read();
        return false;
    }

    this->m_lock->release_read();
    return true;
}

bool chunk::del()
{
    if ( !open_flag )
    {
        return false;
    }

    this->m_lock->lock_write( this->m_operation_flag );
    bool del_flag = this->m_db->Delete( this->m_url );
    this->m_lock->release_write();
    return del_flag;
}

}