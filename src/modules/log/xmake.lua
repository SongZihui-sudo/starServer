add_rules("mode.debug", "mode.release")

add_defines("Log")

target("Logger")    
    add_deps("common")
    set_kind("static")
    add_files("./*.cc")
target_end()

target("Logger-test")    
    set_kind("binary")
    add_files("./test/*.cc")
    add_deps("Logger")
target_end()

