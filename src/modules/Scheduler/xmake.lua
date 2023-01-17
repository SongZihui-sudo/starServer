add_rules("mode.debug", "mode.release")

add_defines("MScheduler")

add_packages("libaco")  -- 使用协程库 libaco

target("scheduler")    
    set_kind("static")
    add_files("./*.cc")
target_end()

target("scheduler-test")    
    set_kind("binary")
    add_files("./test/*.cc")
    add_deps("Logger")
    add_deps("thread")
    add_deps("scheduler")
target_end()