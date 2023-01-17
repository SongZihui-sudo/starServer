add_rules("mode.debug", "mode.release")

add_defines("Common")

target("common")
    set_kind("static")
    add_files("./*.cc")
target_end()   

target("common-test")    
    set_kind("binary")
    add_files("./test/*.cc")
    add_deps("common")
target_end()