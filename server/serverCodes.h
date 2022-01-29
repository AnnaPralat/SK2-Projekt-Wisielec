#pragma once

#define QUEUE_SIZE 5
#define BUFF_SIZE 1024

// message separator and prefixes
#define DATA_SEPARATOR          ";"
#define DATA_END                '\n'

// for entry - with mapping
#define CREATE_USERNAME_PREFIX  "CU"
#define SEND_ROOMS_PREFIX       "SR"
#define CREATE_ROOM_PREFIX      "CR"
#define CHOOSE_ROOM_PREFIX      "CHR"

// for room - when adding player
#define NUM_OF_PLAYERS_PREFIX   "NOP"

// for room
#define START_GAME_PREFIX       "SG"
#define GUESS_LETTER_PREFIX     "GL"
#define END_GAME_PREFIX         "EG"

#define SUCCESS_CODE "0"
#define FAILURE_CODE "1"