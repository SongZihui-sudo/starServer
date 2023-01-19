add_rules("mode.debug", "mode.release")

add_defines("Database")

add_packages("leveldb")     -- 使用 leveldb 库

target("database")
    add_links("sqlite3")
    add_deps("Logger")
    set_kind("static")
    add_files("./*.cc")
target_end()

target("database_test")
    set_kind("binary")
    add_files("./test/*.cc")
    add_deps("database")
target_end()