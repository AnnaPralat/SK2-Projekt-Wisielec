from typing import *

from configClient import PLAYER_LIVES

class PlayerInfo():
    def __init__(self, username: Union[None, str] = None) -> None:
        self.username = username
        self.lives    = PLAYER_LIVES

    def set_player_username(self, username: Union[None, str]) -> None:
        self.username = username
