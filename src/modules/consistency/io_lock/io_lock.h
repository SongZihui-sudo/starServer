#ifndef CHUNK_LOCK_H
#define CHUNK_LOCK_H

#include <inttypes.h>
#include <memory>

#include "modules/meta_data/meta_data.h"

namespace star
{
/*
    io lock 锁
 */
class io_lock
{
protected:
    enum lock_type
    {
        read_lock    = 0,
        write_lock   = 1,
        read_unlock  = 2,
        write_unlock = 3,
        none_lock    = 5
    };

public:
    typedef std::shared_ptr< io_lock > ptr;

    io_lock(){};
    ~io_lock() = default;

    /* 当一个进程获得该文件的锁后，其他进程要访问这个文件要等待解锁 */
public:
    /* 获取读锁 */
    void lock_read( file_operation flag );

    /* 获取写锁 */
    void lock_write( file_operation flag );

    /* 解读锁 */
    void release_read();

    /* 解写锁 */
    void release_write();

public:
    /* 如果文件被锁，锁的类型 */
    lock_type get_lock_type() { return this->m_signal; }

    /* 是否被锁 */
    bool is_lock() { return flag; };

private:
    lock_type m_signal = none_lock;
    bool flag = false;
};
}

#endif