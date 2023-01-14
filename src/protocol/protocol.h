#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "./JsonSerial.h"

#include <memory>
#include <string>
#include <iostream>

namespace star
{
/* 协议 */
class protocol
{
public:
    typedef std::shared_ptr< protocol > ptr;

    struct Protocol_Struct
    {
        int bit;                         /* 标识 */
        std::string from;                /* 请求者ip */
        std::string file_name;           /* 文件名 */
        std::vector< std::string > path; /* 路径 */
        size_t file_size;                /* 大小 */
        std::vector< std::string > data; /* 文件内容 */
    };

public:
    protocol( std::string name, Protocol_Struct buf )
    {
        this->m_name = name;
        js.reset( new JsonSerial() );
        this->m_protocol = buf;
    };
    ~protocol() = default;

public:
    /* 序列化 */
    void Serialize()
    {
#define XX( name )                                                                         \
    serialize( js, name.bit, name.from, name.file_name, name.path, name.file_size, name.data );
        /* 把结构体转成 json */
        XX( this->m_protocol );

#undef XX
    }

    /* 反序列化 */
    void Deserialize()
    {
#define XX( name )                                                                         \
    deserialize( js, name.bit, name.from, name.file_name, name.path, name.file_size, name.data );
        /* 把结构体转成 json */
        XX( this->m_protocol );

#undef XX
    }

    /* 把 json 转换成 string */
    std::string toStr()
    {
        Json::FastWriter styled_writer;
        std::string s = styled_writer.write( js->get() );
        return s;
    }

    /* 转换成C风格字符串 */
    void toCStr( char*& str )
    {
        Json::FastWriter styled_writer;
        std::string s = styled_writer.write( js->get() );
        strcpy( str, s.c_str() );
    }

    /* 字符串 转 json */
    void toJson( std::string str )
    {
        Json::CharReaderBuilder reader;
        std::unique_ptr< Json::CharReader > const json_read( reader.newCharReader() );
        Json::Value tree;
        Json::String err;
        json_read->parse( str.c_str(), str.c_str() + str.length(), &tree, &err );
        js->set( tree );
    }

    void get( Json::Value& val ) { val = js.get(); }

    void display()
    {
        Json::Value::Members mem = js->get().getMemberNames();
        for ( auto iter = mem.begin(); iter != mem.end(); iter++ )
        {
            std::cout << js->get()[*iter];
        }
    }

private:
    Protocol_Struct m_protocol; /* 协议 */
    std::string m_name;         /* 协议名 */
    JsonSerial::ptr js;
};

}

#endif