add_rules("mode.debug", "mode.release")

add_defines("IO_LOCK_XMAKE")

target("io_lock")
    add_deps("Logger")
    set_kind("static")
    add_files("./*.cc")
target_end()

target("io_lock_test")
    set_kind("binary")
    add_files("./test/*.cc")
    add_deps("io_lock")
    add_deps("thread")
target_end()