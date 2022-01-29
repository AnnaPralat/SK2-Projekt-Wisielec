#pragma once

#include <algorithm>
#include <cstring>
#include <random>

#define MAX_NUMBER_OF_PLAYERS 5

class Room {
public:
    bool hasStarted;
    std::string name;
    std::string word;
    std::string guessed;
    std::vector<Player> players;

    Room (const std::string& name) {
        this->name = name;
        this->hasStarted = false;
        this->word = selectWord("words.txt");

        this->guessed = "";
        for (size_t i = 0; i < this->word.length(); ++i) {
            guessed += "*";
        }
    }

    std::string selectWord(std::string filename) {
        std::ifstream myReadFile(filename);
        if (!myReadFile.is_open()) {
            error(1, 0, "Could not find file with words!");
        }
        int lines = std::count(std::istreambuf_iterator<char>(myReadFile),
                    std::istreambuf_iterator<char>(), '\n');

        std::mt19937 gen((std::random_device()()));
        std::uniform_int_distribution<uint8_t> dist(0, lines-1);
        int randomLine = dist(gen);

        myReadFile.clear();
        myReadFile.seekg(0);
        std::string word;
        for (int i = 0; i <= randomLine; ++i) {
            getline(myReadFile, word);
        }
        myReadFile.close();
        
        return word;
    }
};