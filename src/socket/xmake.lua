add_rules("mode.debug", "mode.release")

add_defines("Socket")

target("socket")   
    set_kind("static")
    add_files("./*.cc")
    add_deps("Logger")
target_end()

target("socket-test")    
    set_kind("binary")
    add_deps("socket")
    add_deps("common")
    add_files("./test/main.cc")
target_end()