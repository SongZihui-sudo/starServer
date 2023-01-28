#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <cstddef>
#include <functional>
#include <memory>
#include <stack>
#include <string>

#include "../../modules/Scheduler/scheduler.h"
#include "../../modules/db/database.h"
#include "../../modules/log/log.h"
#include "../../modules/protocol/protocol.h"
#include "../../modules/setting/setting.h"
#include "../../modules/socket/socket.h"
#include "modules/consistency/lease/lease.h"

namespace star
{

static MSocket::ptr remote_sock;
extern std::stack< void* > arg_ss;
extern std::stack< MSocket::ptr > sock_ss;

class tcpserver
{
protected:
    /*服务器状态*/
    enum Status
    {
        Normal        = 0,
        NOT_FOUND     = 1,
        BREAKDOWN     = 2,
        OK            = 3,
        ERROR         = 4,
        COMMAND_ERROR = 5,
        PARSING_ERROR = 6,
        INIT          = 7
    };

public:
    typedef std::shared_ptr< tcpserver > ptr;

    tcpserver( std::filesystem::path settings_path );
    virtual ~tcpserver() = default;

    /* 获取服务器名 */
    std::string get_name() { return this->m_name; }

    /* 获取服务器状态 */
    Status get_status() { return this->m_status; }

public:
    /* 绑定地址 */
    virtual void bind();

    /* 阻塞线程等待连接 */
    virtual void wait( void respond(), void* self );

    /* 服务器关闭 */
    virtual void close();

    /*
        接受消息
        *param remote_sock 必须是accept的socket
    */
    static protocol::ptr recv( MSocket::ptr remote_sock, size_t buffer_size );

    /*
        发送消息
        *patam ——remote_sock 必须是accept的socket
     */
    static int send( MSocket::ptr remote_sock, protocol::Protocol_Struct buf );

protected:
    std::string m_name;                 /* 服务器 */
    Status m_status;                    /* 服务器状态 */
    Logger::ptr m_logger;               /* 日志器 */
    MSocket::ptr m_sock;                /* socket */
    config::ptr m_settings;             /* 服务器设置 */
    levelDB::ptr m_db;                  /* 服务器数据库 */
    protocol::ptr m_protocol;           /* 协议序列化 */
    Scheduler::ptr server_scheduler;    /* 服务器调度期 */
    size_t buffer_size;                 /* 缓冲区的大小 */
    size_t connect_counter;             /* 当前的连接数 */
    lease_manager::ptr m_lease_control; /* 租约管理器 */

private:
    size_t max_connects; /* 最大连接数 */
};
}

#endif