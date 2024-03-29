add_rules("mode.debug", "mode.release")

add_defines("Master_server")

add_packages("leveldb")     -- 使用 leveldb 库
add_packages("jsoncpp")     -- 使用 jsoncpp 库
add_packages("libaco")      -- 使用 libaco 库
add_links("sqlite3")

target("master_server")
    add_deps("timer")
    add_deps("lease")
    add_deps("file")
    add_deps("chunk")
    add_deps("io_lock")
    add_deps("Logger")
    add_deps("database")
    add_deps("scheduler")
    add_deps("tcpserver")
    add_deps("socket")
    add_deps("thread")
    set_kind("binary")  -- 生成二进制文件
    add_files("./*.cc")
    add_files("./main/*.cc")
target_end()