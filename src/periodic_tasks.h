// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#ifndef _PERIODIC_TASKS_H_
#define _PERIODIC_TASKS_H_
#include "config.h"
#include "gb.h"
#define MODE_LOG_FPS 1
#define MODE_NO_LOG_FPS 2
#define BASE_TIMING_INTERVAL 50000 //usec 1/20 sec
#define SEND_PROBE_MULTIPLIER 10 // make send probe some time slower
#ifdef OS_ANDROID
#define LOG_FPS_MULTIPLIER 1 // times of BASE_TIMING_INTERVAL
#else
#define LOG_FPS_MULTIPLIER 1 // times of BASE_TIMING_INTERVAL
#endif
typedef struct {
  double time;
  long long int *bytes_done_conn;
} prev_goodput_meas_t;
typedef struct {
  double start_playback_time; // absolute start playback time (gettime() at the first fps > 0)
  double video_int_time;
} timing_info_t;
typedef struct {
  int mode; // input mode
  timing_info_t timing_info; // output timing info
} log_fps_t;
extern pthread_t update_rtt_tid;
extern pthread_t measure_rtt_tid;
extern pthread_t log_goodput_tid;
extern pthread_t log_fps_tid;
extern pthread_t initiate_periodic_tasks_tid;
extern pthread_t log_goodput_to_client_tid;
extern double global_cur_player_pos;
void *initiate_periodic_tasks(void *);
int start_periodic_tasks(const gb_t *gb);
int set_periodic_init_task(const gb_t *gb);
int start_timer(const gb_t *gb);
int stop_timer(const gb_t *gb);
void *log_fps(void *mode_num);
#endif
