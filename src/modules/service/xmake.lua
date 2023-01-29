add_rules("mode.debug", "mode.release")

add_defines("XMAKE_SERVICE")

target("service_manager")
    add_deps("Logger")
    set_kind("static")
    add_files("./*.cc")
target_end()