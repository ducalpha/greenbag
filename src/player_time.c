// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#include "player_time.h"
#include "util.h"
#include "periodic_tasks.h"
#include "config.h"
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

void *cur_player_pos_shared;
int cur_player_pos_fd = 0;
int cur_player_pos_file_len = 0;
void open_cur_player_pos(void)
{
  if ((cur_player_pos_fd = open(CURRENT_PLAYER_POSITION_FILENAME, O_RDONLY)) == -1)
    perror("WARNING: cannot open current player position file");
  cur_player_pos_file_len = lseek(cur_player_pos_fd, 0, SEEK_END);
  lseek(cur_player_pos_fd, 0, SEEK_SET);
  if ((cur_player_pos_shared = mmap(NULL, cur_player_pos_file_len, PROT_READ, MAP_SHARED, cur_player_pos_fd, 0))
       == MAP_FAILED)
  {
    perror("WARNING: [player_time] failed to mmap current player position");
    return;
  }
}
// TODO: handle the case: gproxy samples too fast
/* return 1: playing, 0: paused, -1: don't know */
int get_player_state(void)
{
  int player_state = UNKNOWN_PLAYER_STATE;
  static double current_time = 0.0;
  static double fps = 0.0;
  static double last_recorded_time = 0.0;
  static long long int last_num_vframe_decoded = 0;
  static long long int num_vframe_decoded = 0;
  if (get_num_vframe_decoded(&num_vframe_decoded) == SUCCESS_STATUS) {
    current_time = gettime();
    fps = (double) (num_vframe_decoded - last_num_vframe_decoded) / (current_time - last_recorded_time);
    //printf("num_vframe_decoded: %lld,", num_vframe_decoded);
    //printf("%lld, %.2lf, %.2lf\n", last_num_vframe_decoded, current_time, last_recorded_time);
    //printf("fps: %.2lf\n", fps);
    if (fps <= LOW_FPS_THRESHOLD)
      player_state = PLAYER_PAUSED;
    else
      player_state = PLAYER_PLAYING;
    last_num_vframe_decoded = num_vframe_decoded;
    last_recorded_time = current_time;
  } else {
    fprintf(stderr, "Error calling dumpsys media.player\n");
  }
  return player_state;
}
void close_cur_player_pos(void)
{
  munmap(cur_player_pos_shared, cur_player_pos_file_len);
  close(cur_player_pos_fd);
}
double get_cur_player_pos(void)
{
  double cur_player_pos = 0.0;
  memcpy(&cur_player_pos, (double *)cur_player_pos_shared, sizeof(double));
#ifndef NDEBUG
  if (cur_player_pos > 0.0)
    printf("Current player position: %.3lf\n", cur_player_pos);
#endif
  return cur_player_pos;
}

long long int estimate_cur_player_pos_offset(gb_t *gb)
{
  return get_cur_player_pos() * gb->qos_bytes_per_second;
}

double get_remaining_playable_time(gb_t *gb)
{
  update_last_downloaded_byte(gb);
  return ((double) gb->last_downloaded_byte / gb->qos_bytes_per_second) - get_cur_player_pos();
}

double get_remaining_playback_time(gb_t *gb)
{
  static double video_duration = 0.0;
  if (video_duration == 0.0) {
    video_duration = (double) gb->size / gb->qos_bytes_per_second;
    printf("Video duration: %.2lf\n", video_duration);
  }
  return video_duration - get_cur_player_pos();
}

