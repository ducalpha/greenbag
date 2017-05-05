// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#ifndef _PLAYER_TIME_H
#include "gb.h"
#define _PLAYER_TIME_H
#define PLAYER_PAUSED 0
#define PLAYER_PLAYING 1
#define UNKNOWN_PLAYER_STATE -1
#define CURRENT_PLAYER_POSITION_FILENAME "current_player_position"
extern void *cur_player_pos_shared;
extern int cur_player_pos_fd;
extern int cur_player_pos_file_len;
void open_cur_player_pos(void);
void close_cur_player_pos(void);
double get_cur_player_pos(void);
int get_player_state(void);
long long int estimate_cur_player_pos_offset(gb_t *gb);
double get_remaining_playable_time(gb_t *gb);
double get_remaining_playback_time(gb_t *gb);
#endif
