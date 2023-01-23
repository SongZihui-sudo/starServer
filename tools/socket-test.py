'''
用来测试 socket 的脚本
'''

import socket
import json
import time


def read_json(path: str) :
    
    with open(path,'r',encoding='utf8')as fp:
        json_data = json.load(fp)

    return str(json_data)


class MSocket:
    def __init__(self, mode, addr: str = "UNKNOW", port: int = 0) -> None:
        self.addr = addr
        self.port = port
        self.m_sock = socket.socket()
        self.remote_addr: str = ""
        self.remote_sock: socket.socket = None
        self.mode = mode

        if mode == 2:
            self.m_sock.bind((addr, port))

    def listen(self, connects: int = 5) -> None:
        self.m_sock.listen(connects)

    def accept(self) -> bool:
        self.remote_sock, self.remote_addr = self.m_sock.accept()
        print("accept connect from %s" % str(self.remote_addr))
        return True

    def send(self, buf: str) -> bool:
        if self.remote_sock != None and self.mode == 2:
            self.remote_sock.send(buf.encode("utf-8"))
            print("Send %s to %s" % (buf, self.remote_addr))
        elif self.mode == 1:
            self.m_sock.send(buf.encode("utf-8"))
            print("Send %s to %s" % (buf, self.addr))
        else:
            print("Please connect first!")

    def recv(self, buflen: int = 1024) -> str:
        buf: str = None
        buf = str(self.remote_sock.recv(buflen).decode("utf-8"))
        if buf:
            print("Get msg %s from %s" % (buf, self.addr))
        return buf

    def close(self):
        self.m_sock.close()

    def connect(self):
        self.m_sock.connect((self.addr, self.port))
        pass

    def server_run(self) -> None:
        self.listen(5)
        self.accept()
        while True:
            buf = self.recv()
            if buf:
                time.sleep(2)
                self.send("Receive OK! what: %s" % buf)
            elif buf == "end":
                self.close()


def main():
    mode: int = int(input("客户端模式(1) or 服务端模式(2)"))

    if mode == 1:
        addr: str = str(input("输入服务器地址："))
        port: int = int(input("输入服务器端口："))
        client = MSocket(mode, addr, port)
        client.connect()

        jsonstr: str = read_json("./tools/protocol.json")
        jsonstr = list(jsonstr)
        
        for i in range(len(jsonstr)):
            if jsonstr[i] == '\'':
                jsonstr[i] = '\"'
        jsonstr = ''.join(jsonstr)
        client.send(jsonstr)

        buf: str = str(input("输入要发送的内容：(输入 End 为结束)："))
        while buf != "End":
            client.send(buf)
            buf: str = str(input("输入要发送的内容：(输入 End 为结束)："))
        client.send("End")

    elif mode == 2:
        addr: str = str(input("输入服务器地址："))
        port: int = int(input("输入服务器端口："))
        server = MSocket(mode, addr, port)
        print("socket server start!")
        server.server_run()
    else:
        print("输入未知的模式！") 

main()
