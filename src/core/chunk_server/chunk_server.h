#ifndef CHUNK_SERVER_H
#define CHUNK_SERVER_H

#include "core/master_server/master_server.h"
#include "core/tcpServer/tcpserver.h"
#include "modules/meta_data/meta_data.h"
#include "modules/protocol/protocol.h"
#include "modules/socket/address.h"

#include <cstdint>
#include <filesystem>
#include <list>
#include <memory>
#include <stack>
#include <star.h>
#include <string>
#include <vector>

namespace star
{
/* Chunk 服务器 */
class chunk_server : public tcpserver
{
protected:
    struct master_server_info
    {
        std::string addr;
        int64_t port;
    };

public:
    typedef std::shared_ptr< chunk_server > ptr;

    chunk_server( std::filesystem::path settins_path );
    ~chunk_server() = default;

public:
    /* 读一个块 */
    virtual void get_chunk( chunk_meta_data c_chunk, std::string& data );

    /* 写一个块 */
    virtual void write_chunk( chunk_meta_data c_chunk );

    /* 删一个块 */
    virtual void delete_chunk( chunk_meta_data c_chunk );

    /* 导出数据库中的元数据 */
    virtual void get_all_meta_data( std::vector< chunk_meta_data >& arr );

    /* 响应 */
    static void respond();

    /* 打印一个字符画 */
    void print_logo()
    {
        DOTKNOW_STD_STREAM_LOG( this->m_logger )
        << "\n  ____    _                    _____       \n"
        << " / ___|  | |_    __ _   _ __  |  ___|  ___ \n"
        << " \\___ \\  | __|  / _` | | '__| | |_    / __|\n"
        << "  ___) | | |_  | (_| | | |    |  _|   \\__ \\\n"
        << " |____/   \\__|  \\__,_| |_|    |_|     |___/\n"
        << "                            ----> Chunk Server Exploded version"
        << "%n%0";
    }

private:
    master_server_info m_master; /* master server 信息 */
    size_t max_chunk_size; /* chunk 的最大大小  */
    std::map< std::string, std::vector< chunk_meta_data > > chunk_list; /* chunk 列表 */
    std::stack< protocol::Protocol_Struct > updo_ss;
};
}

#endif