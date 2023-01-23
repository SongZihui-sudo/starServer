#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "../log/log.h"
#include "./JsonSerial.h"

#include "json/config.h"
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

namespace star
{
/* 协议 */
class protocol
{
public:
    typedef std::shared_ptr< protocol > ptr;

    struct Protocol_Struct
    {
        int bit;                              /* 标识 */
        std::string from;                     /* 请求者ip */
        std::string file_name;                /* 文件名 */
        std::string path;                     /* 路径 */
        int package_size;                     /* 大小 */
        std::string data;                     /* 文件内容 */
        std::vector< std::string > customize; /* 自定义内容 */

        void clear()
        {
            bit = 0;
            from.clear();
            file_name.clear();
            path.clear();
            package_size = 0;
            data.clear();
            customize.clear();
        }
    };

public:
    protocol( std::string name, Protocol_Struct& buf )
    {
        this->m_name = name;
        js.reset( new JsonSerial() );
        this->m_protocol = buf;
        m_logger.reset( STAR_NAME( "protocol_parse" ) );
    };
    ~protocol() = default;

public:
    /* 序列化 */
    void Serialize()
    {
#define XX( name )                                                                         \
    serialize(                                                                             \
    js, name.bit, name.from, name.file_name, name.path, name.package_size, name.data, name.customize );
        /* 把结构体转成 json */

        XX( this->m_protocol );

#undef XX
    }

    /* 反序列化 */
    void Deserialize()
    {
#define XX( name )                                                                         \
    deserialize(                                                                           \
    js, name.bit, name.from, name.file_name, name.path, name.package_size, name.data, name.customize );
        /* 把结构体转成 json */

        XX( this->m_protocol );

#undef XX
    }

    /* 把 json 转换成 string */
    std::string toStr()
    {
        std::string s = js->get().toStyledString();
        return s;
    }

    /* 转换成C风格字符串 */
    void toCStr( const char*& str )
    {
        Json::FastWriter styled_writer;
        std::string s = styled_writer.write( js->get() );
        strcpy( const_cast< char* >( str ), s.c_str() ); /* 拷贝字符串 */
    }

    /* 字符串 转 json */
    bool toJson( std::string str )
    {

        Json::CharReaderBuilder reader;
        std::unique_ptr< Json::CharReader > const json_read( reader.newCharReader() );
        Json::Value tree;
        Json::String err;
        json_read->parse( str.begin().base(), str.end().base(), &tree, &err );
        js->set( tree );

        if ( !err.empty() )
        {
            FATAL_STD_STREAM_LOG( this->m_logger ) << "%D" << err << "%n%0";

            return false;
        }

        this->js->set( tree );

        return true;
    }

    void get( Json::Value& val ) { val = js.get(); }

    void display()
    {
        /*
         Json::Value::Members mem = js->get().getMemberNames();
        for ( auto iter = mem.begin(); iter != mem.end(); iter++ )
        {
            std::cout << *iter << ":" << js->get()[*iter];
        }
        */
        INFO_STD_STREAM_LOG( this->m_logger )
        << "bit:" << S( this->m_protocol.bit ) << "\nfile name:" << this->m_protocol.file_name
        << "\nfile path：" << this->m_protocol.path
        << "\nfile data:" << this->m_protocol.data << "\nfrom: " << this->m_protocol.from
        << "\npackage size: " << S( this->m_protocol.package_size ) << "\nCustomize Info: "
        << "%n%0";
        int i = 0;
        for ( auto item : this->m_protocol.customize )
        {
            INFO_STD_STREAM_LOG( this->m_logger ) << "Customize[" << S( i ) << "]: " << item << "%n%0";
            i++;
        }
    }

    /* 设置协议结构体 */
    void set_protocol_struct( Protocol_Struct in_struct ) { this->m_protocol = in_struct; }

    /* 获取协议结构体 */
    Protocol_Struct get_protocol_struct() { return this->m_protocol; }

private:
    Protocol_Struct m_protocol; /* 协议 */
    std::string m_name;         /* 协议名 */
    JsonSerial::ptr js;
    Logger::ptr m_logger;
};

}

#endif