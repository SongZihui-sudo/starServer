add_rules("mode.debug", "mode.release")

add_defines("XMAKE_LEASE")

add_packages("libaco")

target("lease")
    add_deps("timer")
    set_kind("static")
    add_files("./*.cc")
target_end()