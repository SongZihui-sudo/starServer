add_rules("mode.debug", "mode.release")

add_defines("Protocol")

add_packages("jsoncpp")     -- 使用 jsoncpp

target("serialize-test")    
    set_kind("binary")
    add_deps("Logger")
    add_files("./test/testJsonSerial.cc")
target_end()


target("protocol-test")    
    set_kind("binary")
    add_deps("Logger")
    add_files("./test/testprotocol.cc")
target_end()