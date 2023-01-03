add_rules("mode.debug", "mode.release")

add_defines("Thread")

target("thread")
    set_kind("static")
    add_files("./*.cc")
    add_deps("Logger")
target_end()

target("thread_test")
    set_kind("static")
    add_files("./*.cc")
    add_deps("thread")
target_end()