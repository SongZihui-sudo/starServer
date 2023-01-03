-- add build modes
add_rules("mode.release", "mode.debug")

includes("./src/log")
includes("./src/setting")
includes("./src/thread")

set_languages("c99", "c++17")

-- json cpp 库
add_requires("jsoncpp")