add_rules("mode.debug", "mode.release")

add_defines("Setting")
add_packages("jsoncpp")     -- 使用 jsoncpp 库

target("Settings")   
    set_kind("static")
    add_files("./*.cc")
    add_deps("Logger")
target_end()

target("setting-test")
    set_kind("binary")
    add_files("./test/*.cc")
    add_deps("Settings")
target_end()