/**
 * author: SongZihui-sudo
 * file: database.h
 * desc: 数据库模块
 * date: 2023-1-11
 */

#ifndef DATABASE_H
#define DATABASE_H

#include <cstddef>
#include <filesystem>
#include <functional>
#include <iostream>
#include <leveldb/db.h>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <vector>

#include "../log/log.h"

namespace star
{
/*
    数据库模块
 */
class database
{
public:
    typedef std::shared_ptr< database > ptr;

    database( std::string in_name, std::filesystem::path in_path )
    {
        Logger::ptr temp( new Logger );
        this->m_logger = temp;
        this->m_name   = in_name;
        this->m_logger->set_name( this->m_name + " " + "Logger" );
        this->m_path = in_path;
    }
    ~database(){};

public:
    /*
        获取数据库名
    */
    std::string get_name() { return this->m_name; }

    /*
        设置数据库名
    */
    void set_name( std::string in_name ) { this->m_name = in_name; }

    /*
        设置路径
     */
    void set_path( std::filesystem::path in_path ) { this->m_path = in_path; }

    /*
        获取路径
     */
    std::filesystem::path get_path() { return this->m_path; }

public:
    /*
        打开数据库
    */
    virtual bool open() = 0;

    /*
        关闭数据库
    */
    virtual bool close() = 0;

    /*
        执行数据库命令
    */
    virtual bool
    exec( std::string in_cmd, std::function< int*( void*, int, char**, char** ) > callback = nullptr )
    = 0;

    /*
        命令格式化
    */
    virtual std::string format( const char* pattern, ... ) = 0;

protected:
    std::string m_name;           /* 数据库名 */
    std::filesystem::path m_path; /* 数据库路径 */
    Logger::ptr m_logger;         /* 数据库日志器 */
};

/*
    Sqlite 数据库 子类
 */
class sqlite : public database
{
public:
    typedef std::shared_ptr< sqlite > ptr;

    sqlite( std::string in_name, std::filesystem::path in_path )
    : database( in_name, in_path ){};
    virtual ~sqlite() {}

public:
    virtual bool open() override;  /* 打开数据库 */
    virtual bool close() override; /* 关闭数据库 */
    virtual bool exec( std::string in_cmd,
                       std::function< int*( void*, int, char**, char** ) > callback
                       = nullptr ) override; /* 执行数据库命令 */
    /*
        %c  创建表
        %i  插入
        %e  查
        %u  改
        %l  删
        %n  换行
        %t  tab
        %p  PRIMARY
        %s  字符串
        %d  数字
        %f  浮点数
        %m  数据库名
        %w  where
     */
    virtual std::string format( const char* pattern, ... ) override; /* 格式化数据库命令 */
private:
    sqlite3* m_db;
};

/* leveldb 子类 */
class levelDB : public database
{
public:
    typedef std::shared_ptr< levelDB > ptr;

    levelDB( std::filesystem::path in_path, std::string in_name )
    : database( in_name, in_path )
    {
    }

    virtual ~levelDB()
    {
        /* 关闭数据库 */
        this->close();
    }

public:
    virtual bool open() override;  /* 打开数据库 */
    virtual bool close() override; /* 关闭数据库 */

    /* 因为leveldb为键值形式的数据库，提供了put，get接口，无需使用命令操作，删除这个函数 */
    virtual bool exec( std::string in_cmd,
                       std::function< int*( void*, int, char**, char** ) > callback = nullptr ) override
    {
        FATAL_STD_STREAM_LOG( this->m_logger ) << "This is an invalid function."
                                               << "%n"
                                               << "%0";
        return false;
    } /* 运行数据库命令 */

    /* 因为leveldb为键值形式的数据库，提供了put，get接口，无需使用命令操作，也就无需对字符串进行格式化，于是删除了这个函数 */
    virtual std::string format( const char* pattern, ... ) override
    {
        FATAL_STD_STREAM_LOG( this->m_logger ) << "This is an invalid function."
                                               << "%n"
                                               << "%0";
        return nullptr;
    }; /* 格式化数据库命令 */

    void get_leveldbObj( leveldb::DB*& copy )
    {
        // std::cout << this->m_db;
        copy = this->m_db;
    }

    static std::string joinkey( std::vector< std::string > args )
    {
        std::string key = "";
        for ( auto item : args )
        {
            key += item + "|";
        }
        return key;
    }

public:
public:
    /* 加入一对键值 */
    bool Put( std::string key, std::string value );
    /* 删除一条数据 */
    bool Delete( std::string key );
    /* 读取一条数据 */
    bool Get( std::string key, std::string& value );

private:
    leveldb::DB* m_db;          /* 数据库对象 */
    leveldb::Options m_options; /* 设置 */
};

/*
    由 leveldb 的键值存储抽象出集合
 */
class levelDBList
{
public:
    typedef std::shared_ptr< levelDBList > ptr;

    levelDBList( levelDB::ptr db, std::string obj_name );
    ~levelDBList() = default;

public:
    /*
        从结尾压入
     */
    bool push_back( std::string value );

    /*
        弹出最后一个
     */
    bool pop_back();

    /*
        从开始位置压入
     */
    bool push_front( std::string value );

    /*
        弹出第一个
     */
    bool pop_front();

    /*
        在指定位置插入一个
     */
    bool insert( size_t index, std::string value );

    /*
        删除指定位置的一个元素
     */
    bool remove( size_t index );

    /*
        删除指定键值
     */
    bool remove( std::string value );

    /* 
        查找
     */
    int32_t find(std::string value);

    /*
        长度
     */
    size_t size() { return this->length; }

    /* 
        查看是否与表中的元素重合
     */
    bool is_same(std::string value);

    /* 索引 */
    std::string get( size_t index )
    {
        std::string key   = levelDB::joinkey( { this->m_name, S( index ) } );
        std::string value = "";
        this->m_db->Get( key, value );
        return value;
    }

    /* 重载 operator [] */
    std::string operator[]( size_t index ) { return this->get( index ); }

private:
    std::string m_name; /* 集合的名 */
    size_t length;      /* 集合长 */
    levelDB::ptr m_db;  /* 数据库 */
};

}

#endif