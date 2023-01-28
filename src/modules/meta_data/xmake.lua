add_rules("mode.debug", "mode.release")

add_defines("XMAKE_META_DATA")

add_packages("leveldb")

target("file")
    set_kind("static")
    add_files("./file.cc")
target_end()

target("chunk")
    set_kind("static")
    add_files("./chunk.cc")
target_end()