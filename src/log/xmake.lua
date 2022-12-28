add_rules("mode.debug", "mode.release")

add_defines("Log")

target("test")
    set_kind("binary")
    add_files("./*.cc")
    add_files("./test/*.cc")
target_end()

