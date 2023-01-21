#ifndef CHUNK_SERVER_H
#define CHUNK_SERVER_H

#include "core/tcpServer/tcpserver.h"
#include "modules/meta_data/meta_data.h"

#include <filesystem>
#include <list>
#include <memory>
#include <star.h>
#include <string>
#include <vector>

namespace star
{
/* Chunk 服务器 */
class chunk_server : public tcpserver
{
public:
    typedef std::shared_ptr< chunk_server > ptr;

    chunk_server( std::filesystem::path settins_path );
    ~chunk_server() = default;

public:
    /* 读一个块 */
    virtual void get_chunk( chunk_meta_data c_chunk, const char*& data );

    /* 写一个块 */
    virtual void write_chunk( chunk_meta_data c_chunk );

    /* 删一个块 */
    virtual void delete_chunk( chunk_meta_data c_chunk );

    /* 导出数据库中的元数据 */
    virtual void get_all_meta_data( std::vector< chunk_meta_data >& arr );

    /* 响应 */
    static void respond();

private:
    size_t max_chunk_size; /* chunk 的最大大小  */
    std::map< std::string, std::vector< chunk_meta_data > > chunk_list; /* chunk 列表 */
};
}

#endif