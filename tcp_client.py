import time
from typing import List
import socket
import threading
import select

kULLMaxDigits = 20
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


def GetMsTime() -> int:
    return round(time.time() * 1000)


def RawSend(sock: socket.socket, message: str, size: int):
    if len(message) < size:
        message = message + ("\0" * (size - len(message)))
    sock.send((message[:size]).encode(encoding))


def RawRecv(sock: socket.socket, size: int) -> str:
    return str(sock.recv(size), encoding)


def WaitForData(sock: socket.socket, timeout: float) -> int:
    start_time = GetMsTime()
    read, send, error = select.select([sock], [], [sock], timeout)
    finish_time = GetMsTime()
    if len(error) != 0:
        return -2
    if len(read) == 0:
        return -1

    return finish_time - start_time


class TcpClient:
    def __init__(self, host: str, port: int, ms_ping_threshold=1000, ms_loop_period=100):
        self.__ping_threshold = ms_ping_threshold
        self.__loop_period = ms_loop_period

        self.__heartbeat_socket.connect((host, port))
        RawSend(self.__heartbeat_socket, "0", kULLMaxDigits + 1)

        if WaitForData(self.__heartbeat_socket, self.__ping_threshold) < 0:
            raise TimeoutError("Password waiting timeout")
        password = RawRecv(self.__heartbeat_socket, kULLMaxDigits + 1)

        self.__main_socket.connect((host, port))
        RawSend(self.__main_socket, password, kULLMaxDigits + 1)

        if WaitForData(self.__heartbeat_socket, self.__ping_threshold) < 0:
            raise TimeoutError("Signal waiting timeout")
        if GetNum(RawRecv(self.__main_socket, 1)) != 1:
            raise RuntimeError("Signal is term")

        self.__heartbeat_thread = threading.Thread(target=self.__HeartBeat)
        self.__heartbeat_thread.start()

        self.__constructed = True

    def __del__(self):
        if not self.__constructed:
            self.__main_socket.close()
            self.__heartbeat_socket.close()
            return
        self.StopClient()

    def StopClient(self):
        if not self.__is_active:
            return
        self.__is_active = False
        self.__heartbeat_thread.join()

        self.__main_socket.close()
        self.__heartbeat_socket.close()

    def IsConnected(self) -> bool:
        if not self.__is_active:
            return False
        return self.__ms_ping >= 0

    def IsAvailable(self) -> bool:
        if not self.__is_active and not self.IsConnected():
            self.StopClient()
            raise ConnectionResetError("Peer is not available")

        result = WaitForData(self.__main_socket, 0)
        if result == -1:
            if not self.IsConnected():
                raise ConnectionResetError("Peer is not connected")
            return False
        if result == -2:
            if not self.IsConnected():
                raise ConnectionResetError("Peer is not connected")
            raise RuntimeError("Unknown error while select")

        return True

    def GetPing(self) -> int:
        if self.__ms_ping == -1:
            raise ConnectionResetError("Peer is not connected")
        return self.__ms_ping

    def Send(self, *args):
        if not self.IsConnected():
            raise ConnectionResetError("Peer is not connected")

        data = ToStr(args)
        b_num = FillStr(str(len(data) + 1), kULLMaxDigits + 1)

        message = b_num + data
        RawSend(self.__main_socket, message, len(message) + 1)

    def Receive(self, timeout: int) -> List[str]:
        if not self.__is_active and not self.IsConnected():
            raise ConnectionResetError("Peer is not connected")

        waiting = WaitForData(self.__main_socket, timeout)
        if waiting == -2:
            if not self.IsConnected():
                raise ConnectionResetError("Peer is not connected")
            raise RuntimeError("Unknown error while select")
        if waiting == -1:
            return []

        b_num = GetNum(RawRecv(self.__main_socket, kULLMaxDigits + 1))

        if WaitForData(self.__main_socket, 0) < 0:
            raise RuntimeError("Protocol breaking")

        return RawRecv(self.__main_socket, b_num)[:-1].split(' ')

    def __HeartBeat(self):
        try:
            last_connection = GetMsTime()
            while True:
                start_time = GetMsTime()
                waiting_time = WaitForData(self.__heartbeat_socket, self.__loop_period)
                if not self.__is_active:
                    return
                if waiting_time == -2:
                    self.__ms_ping = -1
                    return
                if waiting_time == -1:
                    if GetMsTime() - last_connection > self.__ping_threshold:
                        self.__ms_ping = -1
                        return
                    continue
                self.__ms_ping = GetNum(RawRecv(self.__heartbeat_socket, kULLMaxDigits + 1))
                recv_time = start_time + waiting_time
                RawSend(self.__heartbeat_socket, str(GetMsTime() - recv_time), kULLMaxDigits + 1)
                last_connection = GetMsTime()
        except:
            self.__ms_ping = -1

    __main_socket = socket.socket()
    __heartbeat_socket = socket.socket()

    __heartbeat_thread: threading.Thread

    __is_active = True

    __ping_threshold: int
    __loop_period: int

    __ms_ping = 0
    __constructed = False
