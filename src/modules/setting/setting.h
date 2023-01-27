/**
 * author: SongZihui-sudo
 * file: setting.h
 * desc: 配置模块
 * date: 2022-12-30
 */

#ifndef SETTING_H
#define SETTING_H

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <json/json.h>

#include "../log/log.h"

namespace star
{
/* 使用 json 来作为配置文件的格式 */
class config
{
public:
    typedef std::shared_ptr< config > ptr;
    config( std::filesystem::path in_path )
    {
        this->m_default_path = in_path;
        this->m_logger.reset( new Logger );
        this->parse();
    }

    ~config(){};

    /*
        解析配置文件
    */
    void parse();

    /*
        保存配置文件
    */
    void fllush();

    /*
        获取指定项的值
    */
    template< class K >
    Json::Value& get( K key );

    /*
        重载 []
    */
    template< class K, class V >
    V& operator[]( K key )
    {
        this->get< K, V >( key );
    };

    /*
        添加指定项的值
    */
    template< class K, class V >
    void add( K key, V value );

    /*
        删除指定值
    */
    template< class K >
    void del( K key );

    /*
        设置路径
    */
    void set_path( std::filesystem::path in_path ) { this->m_default_path = in_path; }

private:
    std::filesystem::path m_default_path = "./setting.json"; /* 默认的配置文件路径 */
    Json::Value m_root;                                      /* 配置文件内容 */
    std::string m_name;                                      /* 对应模块名 */
    Logger::ptr m_logger;                                    /* 日志器 */
};

/* 设置管理器 */
class Setting_Manageer
{
public:
    typedef std::shared_ptr< Setting_Manageer > ptr;

    Setting_Manageer(){};

    ~Setting_Manageer(){};

    /*
        解析相关模块的配置
    */
    void Parse_module( std::string name, std::string path );

    /*
        查找指定模块的设置
    */
    config::ptr find_setting( std::string name );

    /*
        添加模块设置
    */
    void add_setting( std::string name, config::ptr val );

    /*
        删除模块设置
    */
    void del_setting( std::string name );

private:
    std::map< std::string, config::ptr > m_setting_list; /* 模块与相关设置的表 */
    LogManager m_manager;                                /* 日志管理器 */
};

inline void config::parse()
{
    std::ifstream ifs;
    ifs.open( this->m_default_path );

    if ( !ifs.is_open() )
    {
        /* 打开文件失败 */
        ERROR_STD_STREAM_LOG( m_logger ) << "Open File Error!%n"
                                                  << "%0";
        return;
    }
    Json::Reader read;
    if ( !read.parse( ifs, this->m_root ) )
    {
        /* 解析失败 */
        ERROR_STD_STREAM_LOG( m_logger ) << "Parse File Error!%n"
                                                  << "%0";
        return;
    }

    ifs.close();
}

inline void config::fllush()
{
    Json::StreamWriterBuilder writebuild;
    writebuild["emitUTF8"] = true;

    std::string document = Json::writeString( writebuild, this->m_root );

    std::ofstream out;

    out.open( this->m_default_path );
    if ( !out.is_open() )
    {
        ERROR_STD_STREAM_LOG( m_logger ) << "Open File Error!%n"
                                                  << "%0";
        return;
    }

    out << document;
    out.close();
}

template< class K >
inline Json::Value& config::get( K key )
{
    return this->m_root[key];
}

template< class K, class V >
inline void config::add( K key, V value )
{
    this->m_root.get(key);
}

template< class K >
inline void config::del( K key )
{
    this->m_root.removeMember( key );
}

inline void Setting_Manageer::Parse_module( std::string name, std::string path ) 
{
    for ( auto it : this->m_setting_list )
    {
        if ( name == it.first )
        {
            it.second->set_path(path);
            it.second->parse();
        }
    }
}

inline config::ptr Setting_Manageer::find_setting( std::string name )
{
    config::ptr ret = nullptr;
    for ( auto it : this->m_setting_list )
    {
        if ( name == it.first )
        {
            ret = it.second;
            return ret;
        }
    }

    DEBUG_STD_STREAM_LOG( m_manager.get_root() ) << "<< INFO >>"
                                                          << " "
                                                          << "Parse File Error!%n"
                                                          << "%0";

    return ret;
}

inline void Setting_Manageer::add_setting( std::string name, config::ptr val )
{
    this->m_setting_list[name] = val;
}

inline void Setting_Manageer::del_setting( std::string name )
{
    this->m_setting_list.erase( name );
}

};

#endif