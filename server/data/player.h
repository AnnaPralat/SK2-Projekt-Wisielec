#pragma once

#include "../serverCodes.h"
#include <string>

#define PLAYER_LIVES 6

class Player {
public:
    int fd;
    std::string username;
    int points;
    int lives;
    
public:
    Player() {};
    
    Player(const int fd, const std::string& username) {
        this->fd        = fd;
        this->username  = username;
        this->points    = 0;
        this->lives     = PLAYER_LIVES;
    }
};