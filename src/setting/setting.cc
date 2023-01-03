/**
 * author: SongZihui-sudo
 * file: setting.cc
 * desc: 配置模块
 * date: 2022-12-30
 */

#include "./setting.h"

#include <fstream>

#include <json/value.h>

namespace star
{
void config::parse()
{
    std::ifstream ifs;
    ifs.open( this->m_default_path );

    if ( !ifs.is_open() )
    {
        /* 打开文件失败 */
        ERROR_STD_STREAM_LOG( m_logger, DOTKNOW ) << "Open File Error!%n"
                                                  << "%0";
        return;
    }
    Json::Reader read;
    if ( !read.parse( ifs, this->m_root ) )
    {
        /* 解析失败 */
        ERROR_STD_STREAM_LOG( m_logger, DOTKNOW ) << "Parse File Error!%n"
                                                  << "%0";
        return;
    }

    ifs.close();
}

void config::fllush()
{
    Json::StreamWriterBuilder writebuild;
    writebuild["emitUTF8"] = true;

    std::string document = Json::writeString( writebuild, this->m_root );

    std::ofstream out;

    out.open( this->m_default_path );
    if ( !out.is_open() )
    {
        ERROR_STD_STREAM_LOG( m_logger, DOTKNOW ) << "Open File Error!%n"
                                                  << "%0";
        return;
    }

    out << document;
    out.close();
}

template< class K >
Json::Value& config::get( K key )
{
    this->m_root[key];
}

template< class K, class V >
void config::add( K key, V value )
{
    this->m_root.get(key);
}

template< class K >
void config::del( K key )
{
    this->m_root.removeMember( key );
}

void Setting_Manageer::Parse_module( std::string name, std::string path ) 
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

config::ptr Setting_Manageer::find_setting( std::string name )
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

    DEBUG_STD_STREAM_LOG( m_manager.get_root(), DOTKNOW ) << "<< INFO >>"
                                                          << " "
                                                          << "Parse File Error!%n"
                                                          << "%0";

    return ret;
}

void Setting_Manageer::add_setting( std::string name, config::ptr val )
{
    this->m_setting_list[name] = val;
}

void Setting_Manageer::del_setting( std::string name )
{
    this->m_setting_list.erase( name );
}

};