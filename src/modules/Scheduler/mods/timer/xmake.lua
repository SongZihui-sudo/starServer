add_rules("mode.debug", "mode.release")

add_defines("TIMER_XMAKE")

target("timer")
    set_kind("static")
    add_deps("thread")
    add_deps("Logger")
    add_files("./*.cc")
target_end()

target("timer_test")
    set_kind("binary")
    add_deps("timer")
    add_files("./test/*.cc")
target_end()