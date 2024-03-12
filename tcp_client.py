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
    block_size = 1024

    def __init__(self, host: str, port: int, ms_ping_threshold=1000, ms_loop_period=100):
        self.__ping_threshold = ms_ping_threshold
        self.__loop_period = ms_loop_period

        self.__heartbeat_socket.connect((host, port))
        RawSend(self.__heartbeat_socket, "0", kULLMaxDigits + 1)

        if WaitForData(self.__heartbeat_socket, self.__ping_threshold) < 0:
            raise TimeoutError("Password waiting timeout")
        password = RawRecv(self.__heartbeat_socket, kULLMaxDigits + 1)

        self.SetKeepIdle(self.__main_socket)
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
        self.__mutex.acquire()
        self.__is_active = False
        self.__mutex.release()
        self.__heartbeat_thread.join()

        self.__main_socket.close()
        self.__heartbeat_socket.close()

    def IsConnected(self) -> bool:
        self.__mutex.acquire()
        if not self.__is_active:
            self.__mutex.release()
            return False

        ms_ping = self.__ms_ping
        self.__mutex.release()
        return ms_ping >= 0

    def IsAvailable(self) -> bool:
        self.__mutex.acquire()
        is_active = self.__is_active
        self.__mutex.release()

        if not is_active and not self.IsConnected():
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
        self.__mutex.acquire()
        ms_ping = self.__ms_ping
        self.__mutex.release()
        if ms_ping == -1:
            raise ConnectionResetError("Peer is not connected")
        return ms_ping

    def Send(self, *args):
        if not self.IsConnected():
            raise ConnectionResetError("Peer is not connected")

        data = ToStr(args)

        full_block_num = int(len(data) / self.block_size)
        part_block_size = len(data) - (full_block_num * self.block_size)

        RawSend(self.__main_socket, str(full_block_num) + " " + str(part_block_size), (kULLMaxDigits + 1) * 2)

        for i in range(full_block_num):
            RawSend(self.__main_socket, data[i * self.block_size:i * (self.block_size + 1)], self.block_size)

        RawSend(self.__main_socket, data[full_block_num * self.block_size:], part_block_size + 1)

    def Receive(self, timeout: int) -> List[str]:
        self.__mutex.acquire()
        is_active = self.__is_active
        self.__mutex.release()

        if not is_active and not self.IsConnected():
            raise ConnectionResetError("Peer is not connected")

        waiting = WaitForData(self.__main_socket, timeout)
        if waiting == -2:
            if not self.IsConnected():
                raise ConnectionResetError("Peer is not connected")
            raise RuntimeError("Unknown error while select")
        if waiting == -1:
            return []

        control_block = RawRecv(self.__main_socket, (kULLMaxDigits + 1) * 2)
        full_block_num = GetNum(control_block[:control_block.find(' ')])
        part_block_num = GetNum(control_block[control_block.find(' ') + 1:])

        message = ""
        for i in range(full_block_num):
            block = RawRecv(self.__main_socket, self.block_size)
            message = message + block

        part_block = RawRecv(self.__main_socket, part_block_num + 1)
        message = message + part_block[:-1]

        return message.split(' ')

    def GetMsPingThreshold(self):
        return self.__ping_threshold

    def __HeartBeat(self):
        try:
            last_connection = GetMsTime()
            while True:
                start_time = GetMsTime()
                waiting_time = WaitForData(self.__heartbeat_socket, self.__loop_period)

                self.__mutex.acquire()
                if not self.__is_active:
                    self.__mutex.release()
                    return

                if waiting_time == -2:
                    self.__ms_ping = -1
                    self.__mutex.release()
                    return
                if waiting_time == -1:
                    if GetMsTime() - last_connection > self.__ping_threshold:
                        self.__ms_ping = -1
                        self.__mutex.release()
                        return
                    self.__mutex.release()
                    continue
                self.__mutex.release()

                ms_ping = GetNum(RawRecv(self.__heartbeat_socket, kULLMaxDigits + 1))
                self.__mutex.acquire()
                self.__ms_ping = ms_ping
                self.__mutex.release()

                recv_time = start_time + waiting_time
                RawSend(self.__heartbeat_socket, str(GetMsTime() - recv_time), kULLMaxDigits + 1)
                last_connection = GetMsTime()
        except:
            self.__mutex.acquire()
            self.__ms_ping = -1
            self.__mutex.release()

    def SetKeepIdle(self, sock):
        on = 1
        idle_interval = 60
        idle_count = 3

        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, on)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, idle_interval)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, idle_interval)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, idle_count)

    __main_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    __heartbeat_socket = socket.socket()

    __heartbeat_thread: threading.Thread
    __mutex = threading.Lock()

    __is_active = True

    __ping_threshold: int
    __loop_period: int

    __ms_ping = 0
    __constructed = False
