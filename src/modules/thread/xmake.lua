add_rules("mode.debug", "mode.release")

add_defines("Thread")

add_packages("libaco")  -- 使用协程库 libaco
add_packages("jsoncpp")     -- 使用 jsoncpp 库

target("thread")
    add_links("pthread")
    set_kind("static")
    add_files("./thread.cc")

    -- 依赖
    add_deps("Logger")    
    add_deps("common")
target_end()

target("thread_test")
    set_kind("binary")
    add_files("./test/test_thread.cc")
    add_deps("thread")
target_end()