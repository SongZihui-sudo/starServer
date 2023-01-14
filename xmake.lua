-- add build modes
add_rules("mode.release", "mode.debug")

-- 子目标
includes("./src/log")
includes("./src/setting")
includes("./src/thread")
includes("./src/common")
includes("./src/db")
includes("./src/protocol")

-- 配置配置标准
set_languages("c99", "c++20")

-- json cpp 库
add_requires("jsoncpp")
-- leveldb 数据库
add_requires("leveldb")
-- libaco 协程库
add_requires("libaco")
