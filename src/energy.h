// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#ifndef _ENERGY_H_
#define _ENERGY_H_
#include "gb.h"
#define INFINITE_TIME 1e10 //sec
#define DUAL_LINK_MODE 0
#define LTE_SINGLE_LINK_MODE 1
#define WIFI_SINGLE_LINK_MODE 2
#define NUM_LINK_MODE 3
#define LTE_IDLE_STATE 0
#define LTE_TAIL_STATE 1
#define LTE_ACTIVE_STATE 2
#define LTE_TAIL_STATE_TIMEOUT_SEC 11.2
int choose_link_mode(gb_t *gb, int conn_index, long long int next_seg_size);
void get_energy_sorted_link_modes(gb_t *gb, int cur_fin_conn_index, long long int next_seg_size, int energy_sorted_link_modes[3]);
double download_time_next_portion_link_mode(gb_t *gb, long long int next_portion_size, int link_mode);
int does_qos_miss_until_bn_chunk(gb_t *gb, int cur_fin_conn_index);
int does_qmiss_until_next_portion_in_link_mode(gb_t *gb, long long int next_seg_size, int link_mode);
int does_qmiss_until_the_end_of_file_in_dual_link_mode(gb_t *gb);
int update_lte_state(gb_t *gb, double *out_tail_state_start_time);
double get_energy_consumption(gb_t *gb, int cur_fin_conn_index, int link_mode, long long int estimated_lte_goodput, long long int estimated_wifi_goodput, long long int data_size);
#endif
