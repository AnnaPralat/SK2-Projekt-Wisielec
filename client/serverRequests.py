from PyQt5.QtNetwork import QTcpSocket, QAbstractSocket
from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import QMessageBox
from configClient import *
from typing import *

class ServerRequests():
    def __init__(self) -> None:
        self.socket             = QTcpSocket()
        self.timer              = QTimer()
        self.msg_box            = QMessageBox()

        self.socket_setup()

    # socket stuff
    def socket_setup(self) -> None:
        self.connect()

        self.socket.connected.connect(self.on_connected)
        # self.socket.disconnected.connect(self.on_disconnected)
        self.socket.errorOccurred.connect(self.on_error)

    def connect(self) -> None:
        self.timer.setSingleShot(True)
        self.timer.timeout.connect(self.on_timeout)

        self.socket.connectToHost(APP_HOST, APP_PORT)
        self.timer.start(3000)
    
    def on_timeout(self) -> None:
        self.socket.abort()
        self.socket.deleteLater()
        self.socket = None
        
        self.timer.deleteLater()
        self.timer = None

        self.msg_box.about(self.msg_box, 'Error', f'Connect timed out!')
        exit(0)
    
    def on_connected(self) -> None:
        self.timer.stop()
        self.timer.deleteLater()
        self.timer = None

    # def on_disconnected(self) -> None:
    #     print('Server/client disconnected!')
        # self.widget.setEnabled(False)
        # exit(0)

    def on_error(self, error) -> None:
        if error == QAbstractSocket.ConnectionRefusedError or error == QAbstractSocket.RemoteHostClosedError:
            return
        self.msg_box.about(self.msg_box, 'Error', f'Socket error: {self.socket.errorString}')

    # read function
    def on_readyRead(self) -> None:
        response = str(self.socket.readAll(), 'utf8')
        try:
            endIndex = response.index('\x00')
            return response[:endIndex]
        except ValueError:
            return 'Ready read error'

    # send messages
    def send_username(self, username) -> None:
        message = DATA_SEPARATOR.join([CREATE_USERNAME_PREFIX, username, DATA_END])
        self.socket.write(message.encode('utf8'))

    def send_room_list_request(self) -> None:
        message = DATA_SEPARATOR.join([SEND_ROOMS_PREFIX, DATA_END])
        self.socket.write(message.encode('utf8'))

    def send_room_name(self, room_name) -> None:
        message = DATA_SEPARATOR.join([CREATE_ROOM_PREFIX, room_name, DATA_END])
        self.socket.write(message.encode('utf8'))

    def send_chosen_room_name(self, chosen_room_name) -> None:
        message = DATA_SEPARATOR.join([CHOOSE_ROOM_PREFIX, chosen_room_name, DATA_END])
        self.socket.write(message.encode('utf8'))

    def start_game(self, room_name) -> None:
        message = DATA_SEPARATOR.join([START_GAME_PREFIX, room_name, DATA_END])
        self.socket.write(message.encode('utf8'))

    def send_letter(self, letter) -> None:
        message = DATA_SEPARATOR.join([GUESS_LETTER_PREFIX, letter, DATA_END])
        self.socket.write(message.encode('utf8'))