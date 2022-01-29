#pragma once

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <poll.h>
#include <vector>

#include "serverCodes.h"

class ThreadData {
public:
    std::string name;
    std::vector<pollfd> descriptors;
    std::vector<std::string> message;
    bool disconnected;
    int timeout;

    ThreadData(int clientFd) {
        this->name = "";
        this->disconnected = false;
        this->timeout = 2500;
        this->addNewDescriptor(clientFd);
    }

    ~ThreadData() {
        for (size_t i = 0; i < descriptors.size(); ++i) {
            close(descriptors[i].fd);
            // std::cout << "Descriptor " << descriptors[i].fd << " closed" << std::endl;
        }
    }

    // handle descriptors
    void addNewDescriptor(int clientFd) {
        pollfd descr;
        descr.fd = clientFd;
        descr.events = POLLIN;
        descriptors.emplace_back(descr);
        std::string msg = "";
        message.emplace_back(msg);
        // std::cout << "Descriptor " << clientFd << " has been created" << std::endl;
    }

    int findDescriptor(int clientFd) {
        for (size_t i = 0; i < descriptors.size(); ++i) {
            if (descriptors[i].fd == clientFd) {
                return i;
            }
        }
        return -1;
    }

    bool removeDescriptor(int clientFd) {
        int indexToRemove = findDescriptor(clientFd);
        if (indexToRemove != -1) {
            descriptors.erase(descriptors.begin()+indexToRemove);
            // std::cout << "Deleted " << clientFd << " from pollfd structure" << std::endl;
            return true;
        }
        // std::cout << "Couldn't remove given descriptor." << std::endl;
        return false;
    }

    //  handle messages
    void addBuffer(char* buff, int index) {
        message[index] += std::string(buff);
    }

    bool readyRead(int index) {
        return message[index].back() == DATA_END;
    }

    std::string getMessage(int index) {
        std::string result = message[index];
        message[index] = "";
        return result;
    }

    std::string getPrefix(const std::string& message) {
        size_t sepIndex = message.find(DATA_SEPARATOR);
        if (sepIndex == std::string::npos) {
            return "Could not get message prefix";
        }
        return message.substr(0, sepIndex);
    }

    std::string getArguments(std::string& message, int index) {
        message = message.substr(0, message.length()-1);
        size_t sepIndex = message.find(DATA_SEPARATOR);
        if (sepIndex == std::string::npos) {
            return "Could not get message prefix";
        }

        std::string prefix = message.substr(0, sepIndex);
        if (prefix == CREATE_USERNAME_PREFIX || prefix == SEND_ROOMS_PREFIX || prefix == CREATE_ROOM_PREFIX || prefix == CHOOSE_ROOM_PREFIX) {
            return message.substr(sepIndex+1) + std::to_string(descriptors[index].fd);
        }

        return message.substr(sepIndex+1);
    }

    int saveToBuff(char* buff, std::string& message) {
        int bytes = std::min(BUFF_SIZE, (int)message.size());
        memset(buff, 0, BUFF_SIZE);
        sprintf(buff, "%s", message.substr(0, bytes).c_str());
        message = message.substr(bytes);
        return bytes;
    }
};