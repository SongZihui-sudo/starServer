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

target("fiber")
    set_kind("static")
    add_files("./fiber.cc")

    -- 依赖
    add_deps("Logger")    
    add_deps("common")
    add_deps("thread")
target_end()

target("scheduler")
    set_kind("static")
    add_files("./scheduler.cc")

    -- 依赖
    add_deps("Settings")
    add_deps("Logger")
    add_deps("fiber")
    add_deps("common")
    add_deps("thread")
target_end()

target("thread_test")
    set_kind("binary")
    add_files("./test/test_thread.cc")
    add_deps("thread")
target_end()

target("fiber_test")
    set_kind("binary")
    add_files("./test/test_fiber.cc")
    add_deps("fiber")
target_end()

target("scheduler_test")
    set_kind("binary")
    add_files("./test/test_scheduler.cc")
    add_deps("scheduler")
target_end()