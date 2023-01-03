/**
 * author: SongZihui-sudo
 * file: setting.h
 * desc: 配置模块
 * date: 2022-12-30
 */

#ifndef SETTING_H
#define SETTING_H

#include <filesystem>
#include <json/json.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../log/log.h"

using namespace std::filesystem;

namespace star
{
/* 使用 json 来作为配置文件的格式 */
class config
{
public:
    typedef std::shared_ptr< config > ptr;

    config() { this->m_logger.reset(new Logger); };

    config( std::string in_path ) { this->m_default_path = in_path; this->m_logger.reset(new Logger); }

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
    void set_path( std::string in_path ) { this->m_default_path = in_path; }

private:
    std::string m_default_path = "./setting.json"; /* 默认的配置文件路径 */
    Json::Value m_root;                             /* 配置文件内容 */
    std::string m_name;                             /* 对应模块名 */
    Logger::ptr m_logger;                     /* 日志器 */
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
    LogManager m_manager;                          /* 日志管理器 */
};

};

#endif