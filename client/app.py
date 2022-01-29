from multiprocessing.sharedctypes import Value
from functools import partial
from PyQt5.QtWidgets import QApplication, QWidget, QMessageBox, QMainWindow, QTableWidgetItem, QHeaderView, QAbstractButton
from PyQt5.QtGui import QPixmap
from PyQt5.QtCore import Qt
from PyQt5 import uic
from serverRequests import ServerRequests
from playerInfo import PlayerInfo
from roomInfo import RoomInfo
from configClient import *
from typing import *

class App():
    def __init__(self) -> None:
        self.app            = QApplication([])
        self.main_window    = QMainWindow()
        self.widget         = QWidget()
        self.msg_box        = QMessageBox()
        self.server         = ServerRequests()
        self.player         = PlayerInfo()
        self.room           = RoomInfo()
        self.buffer         = ''
        self.end            = False

        # map functions
        self.functionMap = {
            CREATE_USERNAME_PREFIX: self.set_username,
            SEND_ROOMS_PREFIX:      self.display_rooms,
            CREATE_ROOM_PREFIX:     self.set_room,
            CHOOSE_ROOM_PREFIX:     self.set_chosen_room,
            NUM_OF_PLAYERS_PREFIX:  self.set_num_of_players,
            START_GAME_PREFIX:      self.set_game,
            GUESS_LETTER_PREFIX:    self.update_game,
            END_GAME_PREFIX:        self.end_game
        }

        self.server.socket.readyRead.connect(self.on_readyRead)
        self.server.socket.disconnected.connect(self.on_disconnected)
        self.app.aboutToQuit.connect(self.exit)

        self.setup_main_window()

    # main window and load functions
    def setup_main_window(self) -> None:
        uic.loadUi(f'{WIDGET_FOLDER}/{MAIN_WINDOW}', self.main_window)
        self.join_widget()

    def load_widget(self, path: str) -> None:
        self.widget = QWidget()
        uic.loadUi(path, self.widget)
        self.main_window.setCentralWidget(self.widget)

    # read function
    def on_readyRead(self) -> None:
        response = self.server.on_readyRead()
        while DATA_END in response:
            try:
                dataEndIndex = response.index(DATA_END)
                self.buffer += response[:dataEndIndex]
                to_receive   = self.buffer
                self.buffer  = response[dataEndIndex+1:]
                response     = self.buffer
                self.handle_responses(to_receive)
            except ValueError:
                return 'Uncompleted data'
        
    def handle_responses(self, response: str) -> None:
        try:
            response = response.split(';')
            # print(response)
            self.functionMap[response[0]](response[1:])
        except ValueError:
            print('Server error')

    # join widget
    def join_widget(self) -> None:
        self.load_widget(f'{WIDGET_FOLDER}/{WIDGET_JOIN}')

        self.widget.username.textChanged.connect(self.username_changed)
        self.widget.join_button.clicked.connect(self.join_player)
       
        self.main_window.show()

    def username_changed(self, text: str) -> None:
        if text:
            self.widget.join_button.setEnabled(True)
        else:
            self.widget.join_button.setEnabled(False)

    def join_player(self) -> None:
        username = self.widget.username.text()
        self.widget.username.setEnabled(False)
        if DATA_SEPARATOR in username:
            self.msg_box.about(self.msg_box, 'Info', f'Username cannot contain {DATA_SEPARATOR} sign!')
            self.widget.username.setEnabled(True)
        else:
            self.player.set_player_username(username)
            self.server.send_username(username)

    def set_username(self, response) -> None:
        if response[0] == '0':
            self.room_widget()
        else:
            self.msg_box.about(self.msg_box, 'Info', f'Username {self.player.username} is already taken.')
            self.player.set_player_username(None)
            self.widget.username.clear()
            self.widget.username.setEnabled(True)

    # room widget
    def room_widget(self) -> None:
        self.load_widget(f'{WIDGET_FOLDER}/{WIDGET_ROOM}')

        # create room
        self.widget.room_name.textChanged.connect(self.room_name_changed)
        self.widget.create_button.clicked.connect(self.create_room)

        # choose room
        self.widget.rooms_table.itemClicked.connect(self.choose_room)

        self.server.send_room_list_request()

        self.main_window.show()

    def display_rooms(self, response: str) -> None:
        self.widget.rooms_table.setRowCount(len(response)//2)
        self.widget.rooms_table.setColumnCount(2)
        self.widget.rooms_table.setHorizontalHeaderLabels(('Room\'s name', 'Number of players'))
        if response[0] == '0':
            row = 0
            for i in range(1, len(response), 2):
                self.widget.rooms_table.setItem(row, 0, QTableWidgetItem(response[i]))
                self.widget.rooms_table.setItem(row, 1, QTableWidgetItem(response[i+1]))
                row += 1
        self.widget.rooms_table.horizontalHeader().setStretchLastSection(True)
        self.widget.rooms_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)

    def room_name_changed(self, text: str) -> None:
        if text:
            self.widget.create_button.setEnabled(True)
        else:
            self.widget.create_button.setEnabled(False)

    def create_room(self) -> None:
        room_name = self.widget.room_name.text()
        self.widget.room_name.setEnabled(False)
        if DATA_SEPARATOR in room_name:
            self.msg_box.about(self.msg_box, 'Info', f'Room name cannot contain {DATA_SEPARATOR} sign!')
            self.widget.room_name.setEnabled(True)
        else:
            self.room.set_room_name(room_name)
            self.server.send_room_name(room_name)

    def set_room(self, response: str) -> None:
        if response[0] == '0':
            self.wait_widget()
        else:
            self.msg_box.about(self.msg_box, 'Info', f'Room {self.room.room_name} already exists. Choose it from the list')
            self.room.set_room_name(None)
            self.widget.room_name.clear()
            self.widget.room_name.setEnabled(True)

    def choose_room(self) -> None:
        if self.widget.rooms_table.currentColumn() == 0:
            chosen_room_name = self.widget.rooms_table.currentItem().text()
            self.room.set_room_name(chosen_room_name)
            self.server.send_chosen_room_name(chosen_room_name)

    def set_chosen_room(self, response: str) -> None:
        if response[0] == '0':
            self.wait_widget()
        else:
            self.msg_box.about(self.msg_box, 'Info', f'Game has started or too many players in {self.room.room_name} room. Choose another one')
            self.room.set_room_name(None)
    
    def wait_widget(self) -> None:
        self.load_widget(f'{WIDGET_FOLDER}/{WIDGET_WAIT}')

        self.widget.username_label.setText(self.player.username)
        self.widget.room_name_label.setText(self.room.room_name)
        self.widget.num_players_label.setText(str(self.room.num_of_players))
        self.widget.start_button.clicked.connect(self.start_game)

        self.main_window.show()

    def set_num_of_players(self, response) -> None:
        if response[0] == '0':
            self.room.num_of_players = int(response[1])
            self.widget.num_players_label.setText(str(self.room.num_of_players))
            if response[2] == 'FIRST':
                if self.room.num_of_players >= 2:
                    self.widget.start_button.setEnabled(True)
                else:
                    self.widget.start_button.setEnabled(False)
        else:
            self.msg_box.about(self.msg_box, 'Info', f'Server error')
            self.room.set_room_name(None)

    def start_game(self) -> None:
        self.server.start_game(self.room.room_name)

    def set_game(self, response):
        if response[0] == '0':
            self.room.word = response[1]
            for i in range(2, len(response)):
                self.room.players.append([response[i], str(0), str(PLAYER_LIVES)])
            self.game_widget()
        else:
            self.msg_box.about(self.msg_box, 'Info', f'Server error')

    # game widget
    def game_widget(self):
        self.load_widget(f'{WIDGET_FOLDER}/{WIDGET_GAME}')

        # main setup
        self.widget.word_text.setText(self.room.word)
        self.widget.room_name.setText(self.room.room_name)
        self.widget.username.setText(self.player.username)
        self.widget.hangman_img.setPixmap(QPixmap(f'{IMAGE_PATH}{str(0)}{IMAGE_EXTENSION}'))

        # players info
        self.widget.players_table.setRowCount(len(self.room.players))
        self.widget.players_table.setColumnCount(3)
        self.widget.players_table.setHorizontalHeaderLabels(('Nick', 'Points', 'Lives left'))
        row = 0
        for player in self.room.players:
            self.widget.players_table.setItem(row, 0, QTableWidgetItem(player[0]))
            self.widget.players_table.setItem(row, 1, QTableWidgetItem(player[1]))
            self.widget.players_table.setItem(row, 2, QTableWidgetItem(player[2]))
            row += 1
        self.widget.players_table.horizontalHeader().setStretchLastSection(True)
        self.widget.players_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)

        # letters setup
        self.widget.A.clicked.connect(partial(self.letter_clicked, self.widget.A))
        self.widget.B.clicked.connect(partial(self.letter_clicked, self.widget.B))
        self.widget.C.clicked.connect(partial(self.letter_clicked, self.widget.C))
        self.widget.D.clicked.connect(partial(self.letter_clicked, self.widget.D))
        self.widget.E.clicked.connect(partial(self.letter_clicked, self.widget.E))
        self.widget.F.clicked.connect(partial(self.letter_clicked, self.widget.F))
        self.widget.G.clicked.connect(partial(self.letter_clicked, self.widget.G))
        self.widget.H.clicked.connect(partial(self.letter_clicked, self.widget.H))
        self.widget.I.clicked.connect(partial(self.letter_clicked, self.widget.I))
        self.widget.J.clicked.connect(partial(self.letter_clicked, self.widget.J))
        self.widget.K.clicked.connect(partial(self.letter_clicked, self.widget.K))
        self.widget.L.clicked.connect(partial(self.letter_clicked, self.widget.L))
        self.widget.M.clicked.connect(partial(self.letter_clicked, self.widget.M))
        self.widget.N.clicked.connect(partial(self.letter_clicked, self.widget.N))
        self.widget.O.clicked.connect(partial(self.letter_clicked, self.widget.O))
        self.widget.P.clicked.connect(partial(self.letter_clicked, self.widget.P))
        self.widget.Q.clicked.connect(partial(self.letter_clicked, self.widget.Q))
        self.widget.R.clicked.connect(partial(self.letter_clicked, self.widget.R))
        self.widget.S.clicked.connect(partial(self.letter_clicked, self.widget.S))
        self.widget.T.clicked.connect(partial(self.letter_clicked, self.widget.T))
        self.widget.U.clicked.connect(partial(self.letter_clicked, self.widget.U))
        self.widget.V.clicked.connect(partial(self.letter_clicked, self.widget.V))
        self.widget.W.clicked.connect(partial(self.letter_clicked, self.widget.W))
        self.widget.X.clicked.connect(partial(self.letter_clicked, self.widget.X))
        self.widget.Y.clicked.connect(partial(self.letter_clicked, self.widget.Y))
        self.widget.Z.clicked.connect(partial(self.letter_clicked, self.widget.Z))

    def letter_clicked(self, button) -> None:
        letter = button.text()
        button.setEnabled(False)
        self.server.send_letter(letter)

    def disable_guessed(self, letter) -> None:
        for button in self.widget.button_box.findChildren(QAbstractButton):
            if button.text() == letter:
                button.setEnabled(False)

    def update_game(self, response) -> None:
        if response[0] == '0' and len(response) >= 7:
            word = response[1]
            player_stuff = [response[2], response[3], response[4]]
            hit = response[5]
            end = response[6]

            if hit != 'MISS':
                self.disable_guessed(hit)
                self.room.word = word
                self.widget.word_text.setText(self.room.word)
                for i in range(len(self.room.players)):
                    if self.room.players[i][0] == player_stuff[0]:
                        self.room.players[i][1] = player_stuff[1]
                        self.widget.players_table.setItem(i, 1, QTableWidgetItem(self.room.players[i][1]))
            else:
                for i in range(len(self.room.players)):
                    if self.room.players[i][0] == player_stuff[0]:
                        self.room.players[i][2] = player_stuff[2]
                        self.widget.players_table.setItem(i, 2, QTableWidgetItem(self.room.players[i][2]))

                if player_stuff[0] == self.player.username:
                    self.player.lives = player_stuff[2]
                    self.widget.hangman_img.setPixmap(QPixmap(f'{IMAGE_PATH}{str(PLAYER_LIVES-int(self.player.lives))}{IMAGE_EXTENSION}'))
                    if self.player.lives == '0':
                        self.widget.button_box.setEnabled(False)
                        self.widget.end_text.setText('WAIT FOR END')

            if end == 'YES':
                self.end = True
                self.widget.button_box.setEnabled(False)
                self.widget.players_table.sortItems(1, Qt.DescendingOrder)
                self.widget.end_text.setText('THE END')
        else:
            self.msg_box.about(self.msg_box, 'Info', f'Server error')

    def end_game(self, response) -> None:
        if response[0] == '0':
            self.end = True
            self.widget.button_box.setEnabled(False)
            self.widget.players_table.sortItems(1, Qt.DescendingOrder)
            self.widget.end_text.setText('THE END')
            self.msg_box.about(self.msg_box, 'Info', f'Not enough players to continue game. The end.')
        else:
            self.msg_box.about(self.msg_box, 'Info', f'Server error')

    def on_disconnected(self) -> None:
        # self.widget.setEnabled(False)
        # self.msg_box.about(self.msg_box, 'Info', f'Server/client disconnected')
        print('Server/client disconnected!')
        if not self.end:
            exit(0)

    def exit(self) -> None:
        self.main_window.close()
        print('App has been closed')