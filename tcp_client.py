from typing import List
import socket


kIntMaxDigitNum = 10
encoding = 'utf-8'


def FillStr(string: str, size: int) -> str:
    curr_len = len(string)
    for i in range(size - curr_len):
        string = string + '\0'
    return string


def GetNum(string: str) -> int:
    num = 0
    for char in string:
        if not char.isnumeric():
            break
        num = num * 10 + int(char)
    return num


def ToStr(*args) -> str:
    union = ""
    for arg in args[0]:
        if len(union) != 0:
            union = union + " "
        union = union + str(arg)
        print("arg: %s" % str(arg))
    return union


class Socket:
    __socket = socket.socket()

    def __init__(self, host: str, port: int):
        self.__socket.connect((host, port))

    def __del__(self):
        self.__socket.close()

    def Send(self, *args):
        data = ToStr(args)
        b_num = FillStr(str(len(data) + 1), kIntMaxDigitNum + 1).encode(encoding)

        self.__socket.sendall(b_num)
        self.__socket.sendall((data + '\0').encode(encoding))

    def Receive(self) -> List[str]:
        str_num = str(self.__socket.recv(kIntMaxDigitNum + 1), encoding)
        size = GetNum(str_num)

        separated = str(self.__socket.recv(size), encoding)[:-1]
        return separated.split(' ')
