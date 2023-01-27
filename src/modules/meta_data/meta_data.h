#ifndef META_DATA_H
#define META_DATA_H

#include <cstddef>
#include <string>
#include <vector>

#include "../common/common.h"

namespace star
{

enum file_operation
{
    write = -1,
    read  = -2
};

/*
    块元数据
*/
struct chunk_meta_data
{
    std::string f_id;   /* 文件 id */
    std::string f_name; /* 文件名 */
    size_t index;       /* 索引 */
    std::string data;   /* 数据 */
    std::string f_path; /* 文件路径 */
    size_t chunk_size;  /* chunk 大小 */
    std::string from;   /* 所在的 chunk server */
    int port;           /* chunk server 端口 */

    chunk_meta_data( std::string f_name,
                     size_t index,
                     std::string data,
                     std::string path,
                     size_t chunk_size,
                     std::string from,
                     int port )
    {
        this->f_name     = f_name;
        this->index      = index;
        this->data       = data;
        this->f_path     = path;
        this->chunk_size = chunk_size;
        this->from       = from;
        this->port       = port;
    }

    chunk_meta_data() {}
};

/* 文件元数据 */
struct file_meta_data
{
    std::string f_id;                          /* 文件 id */
    std::string f_name;                        /* 文件名 */
    std::vector< chunk_meta_data > chunk_list; /* 文件列表 */
    size_t f_size;                             /* 文件大小 */
    size_t num_chunk;                          /* 文件块数 */
    std::string f_path;                        /* 文件路径 */

    file_meta_data() {}

    file_meta_data( std::string user, std::string f_name, size_t f_size, std::string path )
    {
        f_id         = random_string( 16 );
        this->f_name = f_name;
        this->f_path = path;
        this->f_size = f_size;
    }
};

}
#endif