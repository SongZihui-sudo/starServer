add_rules("mode.debug", "mode.release")

add_defines("XMAKE_TCP_SERVER_")

add_packages("leveldb")
add_packages("jsoncpp")
add_packages("libaco")

target("tcpserver")
    add_deps("service_manager")
    add_deps("scheduler")
    add_files("./*.cc")
    set_kind("static")
target_end()