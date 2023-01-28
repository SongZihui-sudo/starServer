-- add build modes
add_rules("mode.release", "mode.debug")

-- 子目标
includes("./src/modules/log")
includes("./src/modules/setting")
includes("./src/modules/thread")
includes("./src/modules/common")
includes("./src/modules/db")
includes("./src/modules/protocol")
includes("./src/modules/socket")
includes("./src/modules/Scheduler")
includes("./src/core/chunk_server")
includes("./src/core/master_server")
includes("./src/core/tcpServer")
includes("./src/modules/Scheduler/mods/timer")
includes("./src/modules/consistency/io_lock")
includes("./src/modules/meta_data")

add_includedirs("./src/")

-- 配置配置标准
set_languages("c99", "c++20")

-- json cpp 库
add_requires("jsoncpp")
-- leveldb 数据库
add_requires("leveldb")
-- libaco 协程库
add_requires("libaco")
