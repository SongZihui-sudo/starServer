/**
 * author: SongZihui-sudo
 * file: database.cc
 * desc: 数据库模块
 * date: 2023-1-11
 */

#include "./database.h"
#include "../log/log.h"

#include <asm-generic/errno.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

namespace star
{

/* -------------------------------------------------- SQLITE3 ----------------------------------------------- */

bool sqlite::open()
{
    /* 打开数据库 */
    int flag = sqlite3_open( this->m_path.c_str(), &this->m_db );
    if ( flag )
    {
        FATAL_STD_STREAM_LOG( this->m_logger )
        << "Can't open database:" << sqlite3_errmsg( this->m_db ) << "%n"
        << "%0";
        return false;
    }

    INFO_STD_STREAM_LOG( this->m_logger ) << "Open database successfully"
                                          << "%n"
                                          << "%0";
    return true;
}

bool sqlite::close()
{

    /* 关闭数据库 */
    int flag = sqlite3_close( this->m_db );
    if ( flag )
    {
        FATAL_STD_STREAM_LOG( this->m_logger )
        << "Can't open database:" << sqlite3_errmsg( this->m_db ) << "%n"
        << "%0";
        return false;
    }

    INFO_STD_STREAM_LOG( this->m_logger ) << "Close database successfully"
                                          << "%n"
                                          << "%0";
    return true;
}

bool sqlite::exec( std::string in_cmd, std::function< int*( void*, int, char**, char** ) > callback )
{
    char* zErrMsg                               = 0;
    std::string data                            = "Callback function called";
    int ( *func )( void*, int, char**, char** ) = nullptr;
    std::bind( callback, func );

    /* 执行数据库命令 */
    int flag = sqlite3_exec( this->m_db, in_cmd.c_str(), func, ( void* )data.c_str(), &zErrMsg );
    if ( flag != SQLITE_OK )
    {
        FATAL_STD_STREAM_LOG( this->m_logger ) << "SQL error:" << zErrMsg << "%n"
                                               << "%0";
        sqlite3_free( zErrMsg );
        return false;
    }

    INFO_STD_STREAM_LOG( this->m_logger ) << "Operation done successfully"
                                          << "%n"
                                          << "%0";
    return true;
}

std::string sqlite::format( const char* pattern, ... )
{
    va_list ap;
    va_start( ap, pattern );
    std::stringstream in;
    /* 遍历字符串 */
    while ( *pattern )
    {
        if ( *pattern != '%' )
        {
            in << *pattern;
            pattern++;
        }
        else
        {
            pattern++;
#define XX()                                                                               \
    pattern++;                                                                             \
    break;
            switch ( *pattern )
            {
                case 'c':
                    in << "CREATE TABLE";
                    XX();
                case 'i':
                    in << "INSERT INTO";
                    XX();
                case 'e':
                    in << "SELECT";
                    XX();
                case 'u':
                    in << "UPDATE";
                    XX();
                case 'l':
                    in << "DELETE";
                    XX();
                case 'n':
                    in << '\n';
                    XX();
                case 't':
                    in << '\t';
                    XX();
                case 'p':
                    in << "PRIMARY";
                    XX();
                case 's':
                    in << va_arg( ap, char* );
                    XX();
                case 'd':
                    in << va_arg( ap, int );
                    XX();
                case 'f':
                    in << va_arg( ap, double );
                    XX();
                case 'm':
                    in << this->m_name;
                    XX();
                case 'w':
                    in << "WHERE";
                    XX();
            }
#undef XX
        }
    }

    return in.str();
}

/* -------------------------------------------------- LEVELDB ----------------------------------------------- */

bool levelDB::open()
{
    this->m_options.create_if_missing = true; /* 如果数据库不存在就创建一个 */
    leveldb::Status status = leveldb::DB::Open( this->m_options, this->m_path, &this->m_db ); /* 打开数据库 */

    if ( status.ok() )
    {
        INFO_STD_STREAM_LOG( this->m_logger ) << "Open database successfully"
                                              << "%n"
                                              << "%0";
        return true;
    }

    FATAL_STD_STREAM_LOG( this->m_logger ) << "Can't open database:" << status.ToString() << "%n"
                                           << "%0";
    return false;
}

bool levelDB::close()
{
    /* 删除数据库对象 */
    delete this->m_db;
    this->m_db = nullptr;
    return true;
}

bool levelDB::Put( std::string key, std::string value )
{
    leveldb::Status status = this->m_db->Put( leveldb::WriteOptions(), key, value );

    if ( status.ok() )
    {
        INFO_STD_STREAM_LOG( this->m_logger ) << "Put Operation done successfully"
                                              << "%n"
                                              << "%0";
        return true;
    }

    FATAL_STD_STREAM_LOG( this->m_logger ) << "Leveldb Put Error:" << status.ToString() << "%n"
                                           << "%0";
    return false;
}

bool levelDB::Delete( std::string key )
{
    /* 不存在立即删除 */
    leveldb::Status status = this->m_db->Delete( leveldb::WriteOptions(), key );

    if ( status.ok() )
    {
        INFO_STD_STREAM_LOG( this->m_logger ) << "Delete Operation done successfully"
                                              << "%n"
                                              << "%0";
        return true;
    }

    FATAL_STD_STREAM_LOG( this->m_logger ) << "Leveldb Delete Error:" << status.ToString() << "%n"
                                           << "%0";
    return false;
}

bool levelDB::Get( std::string key, std::string& value )
{
    leveldb::Status status = this->m_db->Get( leveldb::ReadOptions(), key, &value );

    if ( status.ok() )
    {
        INFO_STD_STREAM_LOG( this->m_logger ) << "Get Operation done successfully"
                                              << "%n"
                                              << "%0";
        return true;
    }

    FATAL_STD_STREAM_LOG( this->m_logger ) << "Leveldb Get Error:" << status.ToString() << "%n"
                                           << "%0";
    return false;
}

levelDBList::levelDBList( levelDB::ptr db, std::string obj_name )
{
    this->m_db      = db;
    this->m_name    = obj_name;
    std::string len = "";
    std::string key = levelDB::joinkey( { obj_name, "length" } );
    this->m_db->Get( key, len );
    if ( !len.empty() )
    {
        this->length = std::stoi( len );
    }
    else
    {
        this->length = 0;
    }
}

bool levelDBList::push_back( std::string value )
{
    std::string key = levelDB::joinkey( { this->m_name, S( this->length ) } ); /* 拼一下键值 */
    bool flag = this->m_db->Put( key, value );
    if ( flag )
    {
        this->length++;
    }
    key = levelDB::joinkey( { this->m_name, "length" } );
    this->m_db->Put( key, S( length ) );
    return flag;
}

bool levelDBList::pop_back()
{
    std::string key = levelDB::joinkey( { this->m_name, S( this->length ) } ); /* 拼一下键值 */
    bool flag = this->m_db->Delete( key );
    if ( flag )
    {
        this->length--;
    }
    key = levelDB::joinkey( { this->m_name, "length" } );
    this->m_db->Put( key, S( length ) );
    return flag;
}

bool levelDBList::push_front( std::string value )
{
    for ( size_t i = this->length; i > 0; i-- )
    {
        std::string key1 = levelDB::joinkey( { this->m_name, S( i + 1 ) } ); /* 拼一下键值 */
        std::string key2 = levelDB::joinkey( { this->m_name, S( i ) } ); /* 拼一下键值 */
        std::string value = "";
        bool flag         = this->m_db->Get( key2, value );
        if ( !flag )
        {
            return false;
        }
        flag = this->m_db->Put( key1, value );
        if ( !flag )
        {
            return false;
        }
    }
    std::string key = levelDB::joinkey( { this->m_name, S( 0 ) } ); /* 拼一下键值 */
    bool flag       = this->m_db->Put( key, value );
    if ( !flag )
    {
        return false;
    }
    this->length++;
    key = levelDB::joinkey( { this->m_name, "length" } );
    this->m_db->Put( key, S( length ) );
    return true;
}

bool levelDBList::pop_front()
{
    for ( size_t i = 0; i < this->length; i++ )
    {
        std::string key1 = levelDB::joinkey( { this->m_name, S( i + 1 ) } ); /* 拼一下键值 */
        std::string key2 = levelDB::joinkey( { this->m_name, S( i ) } ); /* 拼一下键值 */
        std::string value = "";
        bool flag         = this->m_db->Get( key1, value );
        if ( !flag )
        {
            return false;
        }
        flag = this->m_db->Put( key2, value );
        if ( !flag )
        {
            return false;
        }
    }

    this->length--;
    std::string key = levelDB::joinkey( { this->m_name, "length" } );
    this->m_db->Put( key, S( length ) );
    return true;
}

bool levelDBList::insert( size_t index, std::string value )
{
    for ( size_t i = this->length; i > 0; i-- )
    {
        std::string key1 = levelDB::joinkey( { this->m_name, S( i + 1 ) } ); /* 拼一下键值 */
        std::string key2 = levelDB::joinkey( { this->m_name, S( i ) } ); /* 拼一下键值 */
        std::string value = "";
        bool flag         = this->m_db->Get( key2, value );
        if ( !flag )
        {
            return false;
        }
        flag = this->m_db->Put( key1, value );
        if ( !flag )
        {
            return false;
        }
    }

    std::string key = levelDB::joinkey( { this->m_name, S( index ) } ); /* 拼一下键值 */
    bool flag = this->m_db->Put( key, value );
    if ( !flag )
    {
        return false;
    }

    this->length++;
    key = levelDB::joinkey( { this->m_name, "length" } );
    this->m_db->Put( key, S( length ) );
    return true;
}

bool levelDBList::remove( size_t index )
{
    for ( size_t i = index; i < this->length; i++ )
    {
        std::string key1 = levelDB::joinkey( { this->m_name, S( i + 1 ) } ); /* 拼一下键值 */
        std::string key2 = levelDB::joinkey( { this->m_name, S( i ) } ); /* 拼一下键值 */
        std::string value = "";
        bool flag         = this->m_db->Get( key1, value );
        if ( !flag )
        {
            return false;
        }
        flag = this->m_db->Put( key2, value );
        if ( !flag )
        {
            return false;
        }
    }

    this->length--;
    std::string key = levelDB::joinkey( { this->m_name, "length" } );
    this->m_db->Put( key, S( length ) );
    return true;
}

bool levelDBList::remove( std::string value )
{
    int32_t index = this->find( value );
    if ( index > 0 )
    {
        return this->remove( index );
    }
    return false;
}

int32_t levelDBList::find( std::string value )
{
    int32_t index = -1;
    for ( size_t i = 0; i < this->length; i++ )
    {
        std::string key = levelDB::joinkey( { this->m_name, S( i ) } ); /* 拼一下键值 */
        std::string temp = "";
        bool flag        = this->m_db->Get( key, temp );
        if ( !flag )
        {
            return false;
        }
        if ( temp == value )
        {
            index = i;
            return index;
        }
    }

    return index;
}

}