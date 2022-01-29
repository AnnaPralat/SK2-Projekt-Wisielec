#pragma once

#include <iostream>
#include <vector>
#include <cstring>

#include "data/player.h"
#include "data/room.h"
#include "threadData.h"

class Data {
public:
    std::vector<Player> players;
    std::vector<Room> rooms;

    Data() {}

    // handle player
    void addPlayer(const Player& player) {
        players.emplace_back(player);
    }

    bool findUsername(const std::string& username) {
        for (Player player : players) {
            if (player.username == username) {
                return true;
            }
        }
        return false;
    }

    std::string joinPlayer(std::string data) {
        // std::cout << "joinPlayer function" << std::endl;

        size_t sepIndex = data.find(DATA_SEPARATOR);
        if (sepIndex == std::string::npos) {
            return FAILURE_CODE + std::string(DATA_SEPARATOR) + "Server error!";
        }

        std::string username = data.substr(0, sepIndex);
        int clientFd = atoi(data.substr(sepIndex+1).c_str());
        // std::cout << "Separated data: " << username << " " << clientFd << std::endl;

        bool playerIdx = findUsername(username);
        if (!playerIdx) {
            addPlayer(Player(clientFd, username));
            // std::cout << "Added player: " << clientFd << " " << username << " to Player structure" << std::endl;
            return std::string(CREATE_USERNAME_PREFIX) + std::string(DATA_SEPARATOR) + SUCCESS_CODE
                    + std::string(DATA_SEPARATOR) + "Username doesn't exist";
        }
        return std::string(CREATE_USERNAME_PREFIX) + std::string(DATA_SEPARATOR) + FAILURE_CODE
                + std::string(DATA_SEPARATOR) + "Username already exists!";
    }

    int findPlayer(int& clientFd) {
        for (size_t i = 0; i < players.size(); ++i) {
            if (players[i].fd == clientFd) {
                return i;
            }
        }
        return -1;
    }

    bool deletePlayer(int& clientFd) {
        int indexToRemove = findPlayer(clientFd);
        if (indexToRemove != -1) {
            players.erase(players.begin()+indexToRemove);
            // std::cout << "Deleted " << clientFd << " from Player structure" << std::endl;
            return true;
        }
        // std::cout << "Given player's fd didn't have username yet" << std::endl;
        return false;
    }

    // handle room
    std::string sendRooms(std::string data) {
        // std::cout << "sendRooms function" << std::endl;
        std::string listOfRooms = std::string(SEND_ROOMS_PREFIX) + std::string(DATA_SEPARATOR) + std::string(SUCCESS_CODE);

        for (Room room : rooms) {
            listOfRooms += std::string(DATA_SEPARATOR) + room.name + std::string(DATA_SEPARATOR) + std::to_string(room.players.size());
        }

        return listOfRooms;
    }

    void addRoom(const Room& room) {
        rooms.emplace_back(room);
    }

    int findRoom(const std::string& roomName) {
        for (size_t i = 0; i < rooms.size(); ++i) {
            if (rooms[i].name == roomName) {
                return i;
            }
        }
        return -1;
    }

    bool addPlayerToRoom(int clientFd, const std::string& roomName) {
        int roomIdx   = findRoom(roomName);
        int playerIdx = findPlayer(clientFd);
        if (roomIdx != -1 && playerIdx != -1) {
            rooms[roomIdx].players.emplace_back(players[playerIdx]);
            // std::cout << "Added player " << clientFd << " to room: " << roomName << std::endl;
            return true;
        }
        return false;
    }

    int findPlayerInRoom(int clientFd, int roomIdx) {
        for (size_t i = 0; i < rooms[roomIdx].players.size(); ++i) {
            if (rooms[roomIdx].players[i].fd == clientFd) {
                return i;
            }
        }
        return -1;
    }

    bool deletePlayerFromRoom(int clientFd, const std::string& roomName) {
        int roomIdx = findRoom(roomName);
        if (roomIdx != -1) {
            int playerIdx = findPlayerInRoom(clientFd, roomIdx);
            rooms[roomIdx].players.erase(rooms[roomIdx].players.begin()+playerIdx);
            return true;
        }
        else {
            return false;
        }
    }

    std::string createRoom(std::string data) {
        // std::cout << "createRoom function" << std::endl;

        size_t sepIndex = data.find(DATA_SEPARATOR);
        if (sepIndex == std::string::npos) {
            return FAILURE_CODE + std::string(DATA_SEPARATOR) + "Server error!";
        }

        std::string roomName = data.substr(0, sepIndex);
        int clientFd = atoi(data.substr(sepIndex+1).c_str());
        // std::cout << "Separated data: " << roomName << " " << clientFd << std::endl;

        int roomIdx = findRoom(roomName);
        if (roomIdx == -1) {
            addRoom(Room(roomName));
            addPlayerToRoom(clientFd, roomName);
            
            return std::string(CREATE_ROOM_PREFIX) + std::string(DATA_SEPARATOR) + SUCCESS_CODE 
                    + std::string(DATA_SEPARATOR) + roomName;
        }
        return std::string(CREATE_ROOM_PREFIX) + std::string(DATA_SEPARATOR) + FAILURE_CODE
                + std::string(DATA_SEPARATOR) + "Room already exists!";
    }

    std::string chooseRoom(std::string data) {
        // std::cout << "chooseRoom function" << std::endl;

        size_t sepIndex = data.find(DATA_SEPARATOR);
        if (sepIndex == std::string::npos) {
            return FAILURE_CODE + std::string(DATA_SEPARATOR) + "Server error!";
        }

        std::string roomName = data.substr(0, sepIndex);
        int clientFd = atoi(data.substr(sepIndex+1).c_str());
        // std::cout << "Separated data: " << roomName << " " << clientFd << std::endl;

        int roomIdx = findRoom(roomName);
        if (roomIdx != -1) {
            if (rooms[roomIdx].hasStarted == false && rooms[roomIdx].players.size() < MAX_NUMBER_OF_PLAYERS) {
                addPlayerToRoom(clientFd, roomName);

                return std::string(CHOOSE_ROOM_PREFIX) + std::string(DATA_SEPARATOR) + SUCCESS_CODE
                        + std::string(DATA_SEPARATOR) + roomName;
            }
        }
        return std::string(CHOOSE_ROOM_PREFIX) + std::string(DATA_SEPARATOR) + FAILURE_CODE
                + std::string(DATA_SEPARATOR) + "Server error: Game has started or too many players!";
    }

    bool deleteRoom(const std::string& roomName) {
        int indexToRemove = findRoom(roomName);

        if (indexToRemove != -1) {
            rooms.erase(rooms.begin()+indexToRemove);
            // std::cout << "Deleted " << roomName << " from Room structure" << std::endl;
            return true;
        }

        // std::cout << "Given room's name doesn't exist" << std::endl;
        return false;
    }
};