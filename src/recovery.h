// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#ifndef _RECOVERY_H_
#define _RECOVERY_H_
#include "gb.h"
#include "color.h"
#define SMALL_PORTION_THRESHOLD_FACTOR 10
#define UNDECIDED_RECOVERY_MODE -1
#define NO_RECOVERY_MODE 0
#define RECOVERY_FOR_INSUFFICIENT_PLAYABLE_TIME 1
#define RECOVER_FOR_RESUMING_PLAYING 2
int perform_recovery(gb_t *gb, int conn_index);
int decide_recovery_mode(gb_t *gb, int conn_index);
int recovery_reconnect(gb_t *gb, int another_index);
void return_downloading_for_recovered_conn(gb_t *gb, int index);
int finish_last_chunk_of_subseg(gb_t *gb, int index);
void recover_outorder_portion(gb_t *gb, int conn_index);
void recover_outorder_portion_and_next_segment(gb_t *gb, int conn_index);
void recover_one_time(gb_t *gb, int conn_index);
int get_bottle_neck_conn_index(gb_t *gb);
long long int get_bottle_neck_size(gb_t *gb, int conn_index);
double get_bottle_neck_downloading_time(gb_t *gb, long long int bottle_neck_size);
long long int get_total_goodput(void);
extern int num_recovery_times;
#endif
