add_rules("mode.debug", "mode.release")

add_defines("Setting")
add_packages("jsoncpp")     -- 使用 jsoncpp 库


target("setting-test")    
    add_deps("Logger")
    set_kind("binary")
    add_files("./test/*.cc")
target_end()