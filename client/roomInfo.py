from typing import *

class RoomInfo():
    def __init__(self, room_name: Union[None, str] = None, num_of_players: int = 1) -> None:
        self.room_name      = room_name
        self.num_of_players = num_of_players
        self.word           = ''
        self.players        = []

    def set_room_name(self, room_name: Union[None, str]) -> None:
        self.room_name = room_name
    