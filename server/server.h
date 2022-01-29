#pragma once

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <error.h>
#include <errno.h>

#include "threadData.h"
#include "serverCodes.h"
#include "data.h"

class Server
{
public:
    int serverFd;
    Data data;

    std::vector<std::thread> threads;
    ThreadData* threadEntryData;
    std::vector<ThreadData*> threadData;

    typedef std::string (Data::*function)(std::string);
    std::map<std::string, function> functionMap;

private:
    uint16_t port;

public:
    Server() {
        initServer();

        // map functions for entry thread
        functionMap[CREATE_USERNAME_PREFIX] = &Data::joinPlayer;
        functionMap[SEND_ROOMS_PREFIX]      = &Data::sendRooms;
        functionMap[CREATE_ROOM_PREFIX]     = &Data::createRoom;
        functionMap[CHOOSE_ROOM_PREFIX]     = &Data::chooseRoom;

        std::cout << "Starting server at port number: " << port << std::endl;
    }

    ~Server() {
        // std::cout << "Waiting for threads to join the main process..." << std::endl;
        if (threadEntryData != NULL) {
            threadEntryData->disconnected = true;
        }
        for (size_t i = 0; i < threadData.size(); ++i) {
            threadData[i]->disconnected = true;
        }
        for (size_t i = 0; i < threads.size(); ++i) {
            threads[i].join();
        }
        if (this->threadEntryData != NULL) {
            delete threadEntryData;
        }
        for (size_t i = 0; i < threadData.size(); ++i) {
            delete threadData[i];
        }
        close(serverFd);
        std::cout << "Server has been closed." << std::endl;
    }

    void readConfig(const char* file) {
        std::ifstream myReadFile(file);
        if (!myReadFile.is_open()) {
            error(1, 0, "Could not find config file!");
        }
        std::string text;
        std::getline(myReadFile, text);
        const char *portTxt = text.c_str();
        char *ptr;
        int portTmp = strtol(portTxt, &ptr, 10);
        if (*ptr!=0 || portTmp<1 || (portTmp>((1<<16)-1))) {
            error(1, 0, "Invalid argument: %s", portTxt);
        }
        this->port = portTmp;
        myReadFile.close();
    }

    void checkFunction(int result, const char* errorMessage) {
        if (result == -1) {
            error(1, errno, "%s", errorMessage);
        }
    }

    void initServer() {
        auto configFile = "configServer.txt";
        readConfig(configFile);

        checkFunction(this->serverFd = socket(AF_INET, SOCK_STREAM, 0), "Socket failed!");

        const int one = 1;
        checkFunction(setsockopt(this->serverFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)), "Setsockopt failed!");

        sockaddr_in serverAddr{
            .sin_family = AF_INET,
            .sin_port   = htons((short) this->port),
            .sin_addr   = {INADDR_ANY}
        };

        checkFunction(bind(this->serverFd, (sockaddr*)&serverAddr, sizeof(serverAddr)), "Bind failed!");

        checkFunction(listen(this->serverFd, QUEUE_SIZE), "Listen failed!");
    }

    int acceptConnection() {
        sockaddr_in clientAddr{};
        socklen_t clientAddrSize = sizeof(clientAddr);

        int clientFd;
        checkFunction(clientFd = accept(this->serverFd, (sockaddr*)&clientAddr, &clientAddrSize), "Accept failed!");

        printf("New connection from: %s:%hu (fd: %d)\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), clientFd);
        return clientFd;
    }

    void writeMessage(ThreadData* thData, int clientFd, char* buff, std::string& message) {
        message += '\n';
        while(thData->saveToBuff(buff, message) > 0) {
            if (write(clientFd, buff, BUFF_SIZE) == -1) {
                thData->removeDescriptor(clientFd);
                break;
            }
        }
    }

    void threadRoomFunction(ThreadData* thData) {
        int pollReady;
        char buff[BUFF_SIZE];
        std::string message, prefix;

        while (!thData->disconnected && thData->descriptors.size() > 0) {
            pollReady = poll(&thData->descriptors[0], thData->descriptors.size(), thData->timeout);
            if (pollReady == -1) {
                error(0, errno, "Poll failed!");
                exit(SIGINT);
            }

            for (size_t i = 0; i < thData->descriptors.size() && pollReady > 0; ++i) {
                if (thData->descriptors[i].revents) {
                    auto clientFd = thData->descriptors[i].fd;
                    auto revents = thData->descriptors[i].revents;

                    if (revents & POLLIN) {
                        // std::cout << "Reading data from client: " << clientFd << std::endl;
                        memset(buff, 0, BUFF_SIZE);
                        int bytesRead = read(clientFd, buff, BUFF_SIZE);
                        if (bytesRead <= 0) {
                            // delete player
                            thData->removeDescriptor(clientFd);
                            data.deletePlayer(clientFd);
                            data.deletePlayerFromRoom(clientFd, thData->name);
                            int roomIdx = data.findRoom(thData->name);
                            // if game didn't start - inform other users
                            if (!data.rooms[roomIdx].hasStarted) {
                                // if no players left - delete room
                                if (data.rooms[roomIdx].players.size() < 1) {
                                    data.deleteRoom(data.rooms[roomIdx].name);
                                    thData->disconnected = true;
                                }
                                else {
                                    // update number of players
                                    for (size_t j = 0; j < thData->descriptors.size(); ++j) {
                                        message = NUM_OF_PLAYERS_PREFIX + std::string(DATA_SEPARATOR) + SUCCESS_CODE + std::string(DATA_SEPARATOR)
                                                            + std::to_string(thData->descriptors.size());
                                        if (j == 0) {
                                            message += std::string(DATA_SEPARATOR) + "FIRST";
                                        }
                                        else {
                                            message += std::string(DATA_SEPARATOR) + "WAITING";
                                        }
                                        if (j == thData->descriptors.size()-1) {
                                            sleep(1);
                                        }
                                        writeMessage(thData, thData->descriptors[j].fd, buff, message);
                                    }
                                }
                            }
                            else {
                                // if number of players is less than 2 - end game
                                if (data.rooms[roomIdx].players.size() < 2) {
                                    // end game - delete room
                                    for (size_t j = 0; j < thData->descriptors.size(); ++j) {
                                        message = END_GAME_PREFIX + std::string(DATA_SEPARATOR) + SUCCESS_CODE + std::string(DATA_SEPARATOR) + "END";
                                        writeMessage(thData, thData->descriptors[j].fd, buff, message);
                                    }
                                    data.deleteRoom(data.rooms[roomIdx].name);
                                    thData->disconnected = true;
                                }
                            }
                            // update waiting for choose room info
                            for (size_t j = 0; j < threadEntryData->descriptors.size(); ++j) {
                                message = data.sendRooms("");
                                writeMessage(threadEntryData, threadEntryData->descriptors[j].fd, buff, message);
                            }
                        }
                        else {
                            thData->addBuffer(buff, i);
                            if (thData->readyRead(i)) {
                                message = thData->getMessage(i);
                                prefix  = thData->getPrefix(message);
                                message = thData->getArguments(message, i);
                                
                                if (prefix == START_GAME_PREFIX) {
                                    int index = data.findRoom(thData->name);
                                    data.rooms[index].hasStarted = true;
                                    std::string buildMsg = std::string(START_GAME_PREFIX) + std::string(DATA_SEPARATOR) + SUCCESS_CODE 
                                                            + std::string(DATA_SEPARATOR) + data.rooms[index].guessed;
                                    for (size_t j = 0; j < data.rooms[index].players.size(); ++j) {
                                        buildMsg += std::string(DATA_SEPARATOR) + data.rooms[index].players[j].username;
                                    }

                                    // update waiting for choose room info
                                    for (size_t j = 0; j < thData->descriptors.size(); ++j) {
                                        message = buildMsg;
                                        writeMessage(thData, thData->descriptors[j].fd, buff, message);
                                    }
                                }
                                else if (prefix == GUESS_LETTER_PREFIX) {
                                    char letter = message[0];
                                    int roomIdx = data.findRoom(thData->name);

                                    // update word checking letter hits
                                    int points = 0;
                                    for (size_t j = 0; j < data.rooms[roomIdx].word.length(); ++j) {
                                        if (data.rooms[roomIdx].word[j] == letter && data.rooms[roomIdx].guessed[j] == '*') {
                                            data.rooms[roomIdx].guessed[j] = data.rooms[roomIdx].word[j];
                                            points += 1;
                                        }
                                    }
                                    
                                    // update guessing player state
                                    std::string buildMsg = std::string(GUESS_LETTER_PREFIX) + std::string(DATA_SEPARATOR) + SUCCESS_CODE
                                                            + std::string(DATA_SEPARATOR) + data.rooms[roomIdx].guessed + std::string(DATA_SEPARATOR);
                                    int playerIdx = data.findPlayerInRoom(clientFd, roomIdx);
                                    std::string hit = "MISS";
                                    if (points == 0) {
                                        if (data.rooms[roomIdx].players[playerIdx].lives > 0) {
                                            data.rooms[roomIdx].players[playerIdx].lives -= 1;
                                        }   
                                    }
                                    else {
                                        data.rooms[roomIdx].players[playerIdx].points += points;
                                        hit = letter;
                                    }
                                    // check if it's the end of the game
                                    std::string end = "NO";
                                    if (data.rooms[roomIdx].word == data.rooms[roomIdx].guessed) {
                                        end = "YES";
                                    }
                                    else {
                                        bool atLeastOneAlive = false;
                                        for (size_t j = 0; j < data.rooms[roomIdx].players.size(); ++j) {
                                            if (data.rooms[roomIdx].players[j].lives > 0) {
                                                atLeastOneAlive = true;
                                            }
                                        }
                                        if (!atLeastOneAlive) {
                                            end = "YES";
                                        }
                                    }
                                
                                    buildMsg += data.rooms[roomIdx].players[playerIdx].username + std::string(DATA_SEPARATOR) 
                                                + std::to_string(data.rooms[roomIdx].players[playerIdx].points) + std::string(DATA_SEPARATOR) 
                                                + std::to_string(data.rooms[roomIdx].players[playerIdx].lives) + std::string(DATA_SEPARATOR) 
                                                + hit + std::string(DATA_SEPARATOR) + end;

                                    // send info to all
                                    for (size_t j = 0; j < thData->descriptors.size(); ++j) {
                                        message = buildMsg;
                                        writeMessage(thData, thData->descriptors[j].fd, buff, message);
                                    }
                                    // cleanup
                                    if (end == "YES") {
                                        data.deleteRoom(data.rooms[roomIdx].name);
                                        thData->disconnected = true;
                                        for (size_t j = 0; j < threadEntryData->descriptors.size(); ++j) {
                                            message = data.sendRooms("");
                                            writeMessage(threadEntryData, threadEntryData->descriptors[j].fd, buff, message);
                                        }
                                    }
                                }
                                else {
                                    // std::cout << "Unknown message prefix: " << prefix << std::endl;
                                    message = FAILURE_CODE + std::string(DATA_SEPARATOR) + "Server error!";
                                    writeMessage(thData, clientFd, buff, message);
                                }
                            }
                        }
                        if (revents & ~POLLIN) {
                            // std::cout << "Different revent captured: removing client descriptor..." << std::endl;
                            thData->removeDescriptor(clientFd);
                        }
                    }
                    --pollReady;
                }
            }
        }
        // delete thread
        int index = -1;
        for (size_t i = 0; i < threadData.size(); ++i) {
            if (threadData[i]->name == thData->name) {
                index = i;
            }
        }
        if (index != -1) {
            threadData.erase(threadData.begin()+index);
            // std::cout << "Disconnected " << thData->name << " thread" << std::endl;
        }
        delete thData;
    }

    void handleNewRooms(int clientFd, const std::string& roomName) {
        ThreadData* thData = new ThreadData(clientFd);
        thData->name = roomName;
        threadData.emplace_back(thData);
        threads.emplace_back(std::thread(&Server::threadRoomFunction, this, thData));
        // std::cout << "New ROOM: " << thData->name  << " thread has been created" << std::endl;
    }

    void addToRoomThread(int clientFd, const std::string& roomName) {
        for (size_t i = 0; i < threadData.size(); ++i) {
            if (threadData[i]->name == roomName) {
                threadData[i]->addNewDescriptor(clientFd);
                // std::cout << "Player " << clientFd << " has been assigned to the " << threadData[i]->name << " room" << std::endl;

                // give info to all room members
                char buff[BUFF_SIZE];
                std::string message;
                for (size_t j = 0; j < threadData[i]->descriptors.size(); ++j) {
                    message = NUM_OF_PLAYERS_PREFIX + std::string(DATA_SEPARATOR) + SUCCESS_CODE + std::string(DATA_SEPARATOR)
                                        + std::to_string(threadData[i]->descriptors.size());
                    if (j == 0) {
                        message += std::string(DATA_SEPARATOR) + "FIRST";
                    }
                    else {
                        message += std::string(DATA_SEPARATOR) + "WAITING";
                    }
                    if (j == threadData[i]->descriptors.size()-1) {
                        sleep(1);
                    }
                    writeMessage(threadData[i], threadData[i]->descriptors[j].fd, buff, message);
                }
            }
        }
    }

    void threadFunction(ThreadData* thData) {
        int pollReady;
        char buff[BUFF_SIZE];
        std::string message, prefix;

        while (!thData->disconnected) {
            pollReady = poll(&thData->descriptors[0], thData->descriptors.size(), thData->timeout);
            if (pollReady == -1) {
                error(0, errno, "Poll failed!");
                exit(SIGINT);
            }

            for (size_t i = 0; i < thData->descriptors.size() && pollReady > 0; ++i) {
                if (thData->descriptors[i].revents) {
                    auto clientFd = thData->descriptors[i].fd;
                    auto revents = thData->descriptors[i].revents;

                    if (revents & POLLIN) {
                        // std::cout << "Reading data from client: " << clientFd << std::endl;
                        memset(buff, 0, BUFF_SIZE);
                        int bytesRead = read(clientFd, buff, BUFF_SIZE);
                        if (bytesRead <= 0) {
                            thData->removeDescriptor(clientFd);
                            data.deletePlayer(clientFd);
                        }
                        else {
                            thData->addBuffer(buff, i);
                            if (thData->readyRead(i)) {
                                message = thData->getMessage(i);
                                prefix  = thData->getPrefix(message);
                                message = thData->getArguments(message, i);

                                std::map<std::string, function>::iterator fun = functionMap.find(prefix);
                                if (fun == functionMap.end()) {
                                    // std::cout << "Unknown message prefix: " << prefix << std::endl;
                                    message = FAILURE_CODE + std::string(DATA_SEPARATOR) + "Server error!";
                                }
                                else {
                                    message = (data.*fun->second)(message);
                                }
                                
                                // update rooms structure and information
                                if (prefix == CREATE_ROOM_PREFIX || prefix == CHOOSE_ROOM_PREFIX) {
                                    size_t sepIndex1 = message.find(DATA_SEPARATOR);
                                    size_t sepIndex2 = message.find_last_of(DATA_SEPARATOR);
                                    if (sepIndex1 == std::string::npos || sepIndex2 == std::string::npos) {
                                        std::cout << "Message error" << std::endl;
                                    }
                                    std::string code = message.substr(sepIndex1+1, sepIndex2-sepIndex1-1);
                                    std::string roomName = message.substr(sepIndex2+1, message.length());
                                    writeMessage(thData, clientFd, buff, message);
                                    
                                    if (code == SUCCESS_CODE) {
                                        // update descriptors
                                        if (prefix == CREATE_ROOM_PREFIX) {
                                            handleNewRooms(clientFd, roomName);
                                            thData->removeDescriptor(clientFd);
                                        }
                                        else if (prefix == CHOOSE_ROOM_PREFIX) {
                                            addToRoomThread(clientFd, roomName);
                                            thData->removeDescriptor(clientFd);
                                        }

                                        // update waiting for choose room info
                                        for (size_t j = 0; j < thData->descriptors.size(); ++j) {
                                            message = data.sendRooms("");
                                            writeMessage(thData, thData->descriptors[j].fd, buff, message);
                                        }
                                    }
                                }
                                // no additional actions needed
                                else {
                                    writeMessage(thData, clientFd, buff, message);
                                }
                                
                            }
                        }
                        if (revents & ~POLLIN) {
                            // std::cout << "Different revent captured: removing client descriptor..." << std::endl;
                            thData->removeDescriptor(clientFd);
                        }
                    }
                    --pollReady;
                }
            }
        }
    }

    void handleNewConnection(int clientFd) {
        if (threads.size() == 0) {
            threadEntryData = new ThreadData(clientFd);
            threads.emplace_back(std::thread(&Server::threadFunction, this, threadEntryData));
            // std::cout << "New thread has been created" << std::endl;
        }
        else {
            threadEntryData->addNewDescriptor(clientFd);
            // std::cout << "New player added to entry thread" << std::endl;
        }
    }

    void loop() {
        while (true) {
            int clientFd = this->acceptConnection();

            this->handleNewConnection(clientFd);
        }
    }
};