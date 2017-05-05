// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#include "config.h"
#include <netinet/tcp.h>
#include "periodic_tasks.h"
#include "gb.h"
#include "util.h"
#include "gbsession.h"
#include "ahttpd.h"
#include "config.h"
#include "color.h"
#include "periodic_tasks.h"
#include "player_time.h"
#include <sys/mman.h>
#define NDEBUG
//#define PRINT_CUR_PLAYER_POS
FILE *rtt_log;
FILE *goodput_log;
FILE *goodput_to_client_log;
long long int nsent_bytes_to_client = 0;
pthread_t update_rtt_tid;
pthread_t measure_rtt_tid;
pthread_t log_goodput_tid;
pthread_t log_fps_tid;
pthread_t log_goodput_to_client_tid;
pthread_t initiate_periodic_tasks_tid;
pthread_cond_t update_rtt_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t update_rtt_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t measure_rtt_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t measure_rtt_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t log_goodput_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t log_goodput_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t log_fps_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t log_fps_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t log_goodput_to_client_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t log_goodput_to_client_mutex = PTHREAD_MUTEX_INITIALIZER;
static void *update_rtt(void *arg);
static void *measure_rtt(void *arg);
static void *log_goodput(void *arg);
static void *log_goodput_to_client(void *argc);
int open_goodput_log(gb_t *gb);
int close_goodput_log(gb_t *gb);
int open_rtt_log(gb_t *gb);
int close_rtt_log(gb_t *gb);
void open_cur_player_pos_file(int *cur_player_pos_fd, void **cur_player_pos_shared,
                              double *cur_player_pos, size_t cur_player_pos_len);
void record_cur_player_pos(void *cur_player_pos_shared, double *cur_player_pos, size_t cur_player_pos_len);
void close_cur_player_pos_file(int cur_player_pos_fd, void *cur_player_pos_shared, size_t cur_player_pos_len);
double global_cur_player_pos = 0.0;

// executed every BASIC_TIMEOUT micro sec
void *initiate_periodic_tasks(void *arg)
{
  static long int counter = 0;
  fprintf(stderr, "Initiate periodic task with BASE_TIMING_INTERVAL: %d, LOG_FPS_MULTIPLIER: %d\n", BASE_TIMING_INTERVAL,
            LOG_FPS_MULTIPLIER);
  while (gb) {
    usleep(BASE_TIMING_INTERVAL);
#if ENABLE_UPDATE_RTT
    if ((counter % SEND_PROBE_MULTIPLIER) == 0) {
      pthread_mutex_lock(&update_rtt_mutex);
      pthread_cond_signal(&update_rtt_cond);
      pthread_mutex_unlock(&update_rtt_mutex);
    }
#endif
#if ENABLE_MEASURE_RTT
    pthread_mutex_lock(&measure_rtt_mutex);
    pthread_cond_signal(&measure_rtt_cond);
    pthread_mutex_unlock(&measure_rtt_mutex);
#endif
#if ENABLE_LOG_GOODPUT
    pthread_mutex_lock(&log_goodput_mutex);
    pthread_cond_signal(&log_goodput_cond);
    pthread_mutex_unlock(&log_goodput_mutex);
#endif
#if ENABLE_LOG_GOODPUT_TO_CLIENT
    pthread_mutex_lock(&log_goodput_to_client_mutex);
    pthread_cond_signal(&log_goodput_to_client_cond);
    pthread_mutex_unlock(&log_goodput_to_client_mutex);
#endif
    if ((counter % LOG_FPS_MULTIPLIER) == 0) {
      pthread_mutex_lock(&log_fps_mutex);
      pthread_cond_signal(&log_fps_cond);
      pthread_mutex_unlock(&log_fps_mutex);
    }
    ++counter;
  }
  fprintf(stderr, "initiate periodic tasks finished\n");
  return NULL;
}
/* return a status */
int start_periodic_tasks(const gb_t *gb)
{
#if ENABLE_UPDATE_RTT
  if (pthread_create(&update_rtt_tid, NULL, update_rtt, NULL) != 0) {
    perror("pthread_create, send_rtt_probe");
    return FAILURE_STATUS;
  }
#endif
#if ENABLE_MEASURE_RTT
  if (pthread_create(&measure_rtt_tid, NULL, measure_rtt, NULL) != 0) {
    perror("pthread_create, measure_rtt");
    return FAILURE_STATUS;
  }
#endif
#if ENABLE_LOG_GOODPUT
  if (pthread_create(&log_goodput_tid, NULL, log_goodput, NULL) != 0) {
    perror("pthread_create, log_goodput");
    return FAILURE_STATUS;
  }
#endif
  /*if (pthread_create(&log_fps_tid, NULL, log_fps, NULL) != 0) {
    perror("pthread_create, log_fps");
    return FAILURE_STATUS;
  }*/
  if (pthread_create(&initiate_periodic_tasks_tid, NULL, initiate_periodic_tasks, NULL) != 0) {
    perror("pthread_create, initiate_periodic_tasks");
    return FAILURE_STATUS;
  }
#if ENABLE_LOG_GOODPUT_TO_CLIENT
  if (pthread_create(&log_goodput_to_client_tid, NULL, log_goodput_to_client, NULL) != 0) {
    perror("pthread_create, log_goodput_to_client");
    return FAILURE_STATUS;
  }
#endif
  return SUCCESS_STATUS;
}
static void *measure_rtt(void *arg)
{
  struct tcp_info tcpinfo;
  socklen_t tcpinfo_len = sizeof(tcpinfo);
  double rtt, rtt_dev; //rtt deviation
  int i;
  open_rtt_log(gb);
  while (gb && !gb->finish) {
    pthread_mutex_lock(&measure_rtt_mutex);
    pthread_cond_wait(&measure_rtt_cond, &measure_rtt_mutex);
    pthread_mutex_unlock(&measure_rtt_mutex);

    fprintf(rtt_log, "%.2f", gettime() - gb->start_time);
#ifndef NDEBUG
    printf("\n%.2f, ", gettime() - gb->start_time);
#endif
    for (i = 0; i < gb->conf->num_connections; ++i) {
      if (gb->conn[i].enabled && (gb->conn[i].fd != 0)) {
        if (getsockopt(gb->conn[i].fd, SOL_TCP, TCP_INFO, &tcpinfo, &tcpinfo_len) != -1) {
          rtt = (double)tcpinfo.tcpi_rtt / 1000.0;
          rtt_dev = (double)tcpinfo.tcpi_rttvar / 1000.0;
          fprintf(rtt_log,"\t%.2f\t%.2f", rtt, rtt_dev);
#ifndef NDEBUG
          printf("conn[%i] RTT: %.2f(ms) RTT Dev: %.2f(ms)", i, rtt, rtt_dev);
#endif
        } else {
#ifndef NDEBUG
          printf("%.f2\tFailed to get tcp info\n", gettime() - gb->start_time);
#endif
        }
      } else {
        fprintf(rtt_log,"\t\t");
      }
    }
    fprintf(rtt_log,"\n");
#ifndef NDEBUG
    printf("\n");
#endif
    fflush(rtt_log);
  }
  close_rtt_log(gb);
  return NULL;
}

static void *update_rtt(void *arg)
{
  static char rtt_probe[] = "\r\n\r\n\r\n\r\n"; // 8 bytes + 1 byte of NULL
  ssize_t sent_bytes;
  int i;
  int *rtt_meas_conn = NULL;
	struct tcp_info tcpinfo;
	socklen_t tcpinfo_len = sizeof(tcpinfo);
  // this function must be called after update gb->conf->num_connections
  int num_connections = 0;
  while (!gb || !gb->conf) {
    usleep(500000);
    fprintf(stderr, "Waiting for gb to be initialized\n");
  }
  num_connections = gb->conf->num_connections;
  rtt_meas_conn = malloc(sizeof(int) * num_connections);
  // make new connections
  for (i = 0; i < num_connections; ++i)
  {
    while (!gb || !gb->conn[i].local_if || (strlen(gb->conn[i].local_if) == 0)) {
      usleep(500000);
      fprintf(stderr, "\nWaiting for connection[%d]'s local_if to be initialized\n", i);
    }
    printf("\nMake an rtt-meas-connection to server %s:%d, via interface %s \n", gb->conn[i].host, gb->conn[i].port, gb->conn[i].local_if);
    if ((rtt_meas_conn[i] = tcp_connect(gb->conn[i].host, gb->conn[i].port, gb->conn[i].local_if)) == -1)
      fprintf(stderr, "Failed to make connection to server %s:%d, via interface %s \n", gb->conn[i].host, gb->conn[i].port, gb->conn[i].local_if);
  }
  // send probe periodically
  // update gb->conn[i].rtt and .rtt_dev periodically
  // if the connection closes, reconnect, hopefully it would not block
  while (gb && !gb->finish) {
    pthread_mutex_lock(&update_rtt_mutex);
    pthread_cond_wait(&update_rtt_cond, &update_rtt_mutex);
    pthread_mutex_unlock(&update_rtt_mutex);
    for (i = 0; i < num_connections; ++i)
    {
      if (gb->conn[i].enabled && (rtt_meas_conn[i] != 0))
      {
        sent_bytes = send(rtt_meas_conn[i], rtt_probe, sizeof(rtt_probe)/sizeof(char) - 1, MSG_NOSIGNAL);
        if (sent_bytes < 0) {
          perror("sending rtt probe failed!");
          printf("%c[%d;%d;%dmTry to reconnect rtt-meas-conn[%d]%c[%d;%dm\n", 0x1B, BRIGHT, RED, BG_BLACK, i, 0x1B, WHITE, ATTR_OFF);
          Close(rtt_meas_conn[i]);
          rtt_meas_conn[i] = tcp_connect(gb->conn[i].host, gb->conn[i].port, gb->conn[i].local_if);
        } else {
#if 1
#ifdef OS_ANDROID
          fprintf(stderr, "%.2f, sent rtt probe: %ld (bytes) of %u elements\n", gettime(), sent_bytes, sizeof(rtt_probe)/sizeof(char));
#else
          fprintf(stderr, "%.2f, sent rtt probe: %ld (bytes) of %lu elements\n", gettime(), sent_bytes, sizeof(rtt_probe)/sizeof(char));
#endif
#endif
        }
      }
    }
    for (i = 0; i < num_connections; ++i)
    {
      if (gb->conn[i].enabled && (gb->conn[i].fd != 0))
      {
        if (getsockopt(gb->conn[i].fd, SOL_TCP, TCP_INFO, &tcpinfo, &tcpinfo_len) != -1) 
        {
          gb->conn[i].rtt = (double)tcpinfo.tcpi_rtt / 1000.0;
          gb->conn[i].rtt_dev = (double)tcpinfo.tcpi_rttvar / 1000.0;
          printf("conn[%i] RTT: %.2f(ms) RTT Dev: %.2f(ms)\n", i, gb->conn[i].rtt, gb->conn[i].rtt_dev);
        }
      }
    }
  }
  //close rtt measurement connections
  for (i = 0; i < num_connections; ++i)
    Close(rtt_meas_conn[i]);
  return NULL;
}
// TODO add measure RSSI here
static void *log_goodput(void *arg)
{
  static prev_goodput_meas_t prev_goodput_meas;
  int i;
  double current_time;
  double goodput;
  prev_goodput_meas.bytes_done_conn = malloc(gb->conf->num_connections * sizeof(long long int));
  for (i = 0; i < gb->conf->num_connections; ++i) {
    prev_goodput_meas.time = 0.0;
    prev_goodput_meas.bytes_done_conn[i] = 0;
  }
  open_goodput_log(gb);
  while (gb && !gb->finish) {
    pthread_mutex_lock(&log_goodput_mutex);
    pthread_cond_wait(&log_goodput_cond, &log_goodput_mutex);
    pthread_mutex_unlock(&log_goodput_mutex);

    current_time = gettime() - gb->start_time;
    fprintf(goodput_log, "%.2f", current_time);
#ifndef NDEBUG
    printf("\n%.2f", current_time);
#endif
    for (i = 0; i < gb->conf->num_connections; ++i) {
      if (gb->conn[i].enabled) {
        goodput = (double)(gb->conn[i].bytes_done - prev_goodput_meas.bytes_done_conn[i]) / (current_time - prev_goodput_meas.time) * 8.0 / 1e6;
        fprintf(goodput_log, "\t%.1f", goodput);
#ifndef NDEBUG
        printf("\t%.1f\t", goodput);
#endif
        prev_goodput_meas.bytes_done_conn[i] = gb->conn[i].bytes_done;
      } else {
        fprintf(goodput_log, "\tthis link is disabled");
      }
    }
    prev_goodput_meas.time = current_time;
    fprintf(goodput_log, "\n");
#ifndef NDEBUG
    printf("\n");
#endif
    fflush(goodput_log);
  }
  close_goodput_log(gb);
  return NULL;
}
int open_goodput_log(gb_t *gb)
{
  int i;
  char goodput_log_filepath[512];
  if ((goodput_log = fopen(get_unique_filepath(goodput_log_filepath, GOODPUT_LOG_FILEPATH), "w")) == NULL) {
    fprintf(stderr, "Could not create goodput log file: %s\n", GOODPUT_LOG_FILEPATH);
    exit(1);
  }
  fprintf(goodput_log, "Time(s)");
  for (i = 0; i < gb->conf->num_connections; ++i) {
    fprintf(goodput_log, "\tGoodput[%d](Mbps)", i);
  }
  fprintf(goodput_log, "\n");
  return SUCCESS_STATUS;
}
int close_goodput_log(gb_t *gb)
{
  fclose(goodput_log);
  return SUCCESS_STATUS;
}
/*
 * Open and write the first line of the rtt log
 */
int open_rtt_log(gb_t *gb)
{
  int i;
  char rtt_log_filepath[512];
  if ((rtt_log = fopen(get_unique_filepath(rtt_log_filepath, RTT_LOG_FILEPATH), "w")) == NULL) {
    fprintf(stderr, "Could not create rtt log file: %s\n", RTT_LOG_FILEPATH);
    exit(1);
  }
  fprintf(rtt_log, "Time(s)");
  for (i = 0; i < gb->conf->num_connections; ++i) {
    fprintf(rtt_log, "\tRTT[%d](ms)\tRTT Dev[%d](ms)", i, i);
  }
  fprintf(rtt_log, "\n");
  return SUCCESS_STATUS;
}
int close_rtt_log(gb_t *gb)
{
  fclose(rtt_log);
  return SUCCESS_STATUS;
}
/* return video interruption time as a double */
void *log_fps(void *log_fps_arg)
{
  log_fps_t *log_fps_param = (log_fps_t *)log_fps_arg;
  char fps_log_filepath[512];
  int mode = MODE_LOG_FPS;
  if (log_fps_param && (log_fps_param->mode != 0))
    mode = log_fps_param->mode;
  FILE *fps_log = NULL;
  long long int num_vframe_decoded = 0;
  double current_time = 0.0;
  double fps = 0.0;
  double last_recorded_time = 0.0;
  long long int last_num_vframe_decoded = 0;
  timing_info_t timing; // for return values
  int stop_after_transferring = 0; 
  double video_int_time_added_at_tail = 0.0;
  double fps_sampling_interval = (double) (BASE_TIMING_INTERVAL * LOG_FPS_MULTIPLIER) / 1e6;
  timing.video_int_time = 0.0; timing.start_playback_time = 0.0;
  int cur_player_pos_fd = 0;
  void *cur_player_pos_shared = NULL;
  double cur_player_pos = 0.0;
  size_t cur_player_pos_len = sizeof(cur_player_pos);
  double video_initial_start_time = 0.0;
  open_cur_player_pos_file(&cur_player_pos_fd, &cur_player_pos_shared, &cur_player_pos, cur_player_pos_len);
  if (mode == MODE_LOG_FPS) {
    if ((fps_log = fopen(get_unique_filepath(fps_log_filepath, FPS_LOG_FILEPATH), "w")) == NULL) {
      fprintf(stderr, "error create log FPS\n");
      return NULL;
    }
    fprintf(fps_log, "Time\tFrames per second\tNumber of video frames decoded\n");
  }
  while ((gb && !gb->finish) || !stop_after_transferring) {
    pthread_mutex_lock(&log_fps_mutex);
    pthread_cond_wait(&log_fps_cond, &log_fps_mutex);
    pthread_mutex_unlock(&log_fps_mutex);
    if (get_num_vframe_decoded(&num_vframe_decoded) == SUCCESS_STATUS) {
      current_time = gettime();
      fps = (double) (num_vframe_decoded - last_num_vframe_decoded) / (current_time - last_recorded_time);
      if ((fps > 0) && (timing.start_playback_time == 0.0)) {
        timing.start_playback_time = current_time;
        video_initial_start_time =  timing.start_playback_time - gb->start_time;
        fprintf(stderr, "start play back time: %.2f sec\n", video_initial_start_time);
      }
      if (gb && !gb->finish && (timing.start_playback_time > 0.0))
      {
        if (fps <= LOW_FPS_THRESHOLD)
        {
          //timing.video_int_time += (current_time - last_recorded_time) / 1e6;// this can increase video_int_time
          timing.video_int_time += fps_sampling_interval;
          video_int_time_added_at_tail += fps_sampling_interval;
          //printf("Video Interruption Time: %.2f\n", timing.video_int_time);
          printf(".");
        }
        else
        {
#ifdef OS_ANDROID
          cur_player_pos = (double) num_vframe_decoded / VIDEO_FPS;
#else
          cur_player_pos += fps_sampling_interval;
#endif
          record_cur_player_pos(cur_player_pos_shared, &cur_player_pos, cur_player_pos_len);
          global_cur_player_pos = cur_player_pos;
#ifdef PRINT_CUR_PLAYER_POS
          printf("Current Player Position: %.2f\n", cur_player_pos);
#endif
        }
      }
      if (gb && !gb->finish && fps > LOW_FPS_THRESHOLD)
      {
        video_int_time_added_at_tail = 0.0;
      }
      if (mode == MODE_LOG_FPS)
      {
        fprintf(fps_log, "%.2f\t%.1f\t%lld\n", current_time - gb->start_time, fps , num_vframe_decoded);
      }
      /*fprintf(stderr, "%.2f\tfps:%.3f\tnum vframes decoded:%lld\tvideo int time:%.2f\tcurrent time:%.2f\tlast_recorded_time:%.2f\n",
                      current_time - gb->start_time, fps , num_vframe_decoded, timing.video_int_time
                      ,current_time, last_recorded_time);*/
      fflush(fps_log);
      last_recorded_time = current_time;
      last_num_vframe_decoded = num_vframe_decoded;
      if (gb && gb->finish && (fps == 0))
        stop_after_transferring = 1;
    } else {
      fprintf(stderr, "%.2f\terror calling dumpsys media.player\n", gettime() - gb->start_time);
      //fprintf(fps_log, "%.2f\terror calling dumpsys media.player\n", gettime() - gb->start_time);
      //fflush(fps_log);
    }
  }
  if (mode == MODE_LOG_FPS) { 
    fclose(fps_log);
  }
  close_cur_player_pos_file(cur_player_pos_fd, cur_player_pos_shared, cur_player_pos_len);
  printf("Current player position: %.2f\n", cur_player_pos);
  printf("Video interruption time added at tail: %.2f\n", video_int_time_added_at_tail);
  fprintf(stderr, "log_fps finishes\n");
  if (log_fps_param) {
    if (timing.start_playback_time == 0.0) // video not started yet
      log_fps_param->timing_info.start_playback_time = gettime();
    else
      log_fps_param->timing_info.start_playback_time = timing.start_playback_time;
    log_fps_param->timing_info.video_int_time = timing.video_int_time - video_int_time_added_at_tail;
  }
  return NULL;
}
static void *log_goodput_to_client(void *argc)
{
  char goodput_to_client_log_filepath[512];
  long long int first_sent_byte;
  double current_time;
  double prev_time;
  double goodput;
  long long int prev_sent_byte;
  prev_sent_byte = first_sent_byte = nsent_bytes_to_client;
  prev_time = current_time = gettime() - gb->start_time;
  if ((goodput_to_client_log = fopen(get_unique_filepath(goodput_to_client_log_filepath, GOODPUT_TO_CLIENT_LOG_FILEPATH), "w")) == NULL) {
    fprintf(stderr, "error create log goodput to client\n");
    return NULL;
  }
  fprintf(goodput_to_client_log, "Time\tGoodput(Mbps)\tNum sent bytes\n");
  while (gb && !gb->finish) {
    pthread_mutex_lock(&log_goodput_to_client_mutex);
    pthread_cond_wait(&log_goodput_to_client_cond, &log_goodput_to_client_mutex);
    pthread_mutex_unlock(&log_goodput_to_client_mutex);

    current_time = gettime() - gb->start_time;
    goodput = (double) (nsent_bytes_to_client - prev_sent_byte) / (current_time - prev_time) * 8.0 / 1e6;
    fprintf(goodput_to_client_log, "%.2f\t%.2f\t%lld\n", current_time, goodput, nsent_bytes_to_client - first_sent_byte);
    fflush(goodput_to_client_log);
    prev_sent_byte = nsent_bytes_to_client;
    prev_time = gettime() - gb->start_time;
#ifndef NDEBUG
    printf("\n%.2f, goodput to client: %.2f, num sent bytes: %lld\n", current_time, goodput, nsent_bytes_to_client - first_sent_byte);
#endif
  }
  fclose(goodput_to_client_log);
  return NULL;
}
/* output cur_player_pos_fd and cur_player_shared */
void open_cur_player_pos_file(int *cur_player_pos_fd, void **cur_player_pos_shared,
                              double *cur_player_pos, size_t cur_player_pos_len)
{
  // open using shared memory
  if ((*cur_player_pos_fd = open(CURRENT_PLAYER_POSITION_FILENAME, O_CREAT | O_RDWR | O_TRUNC, 0666)) == -1)
  {
    perror("WARNING: failed to create current player position file");
    return;
  }
  if ((*cur_player_pos_shared = mmap(NULL, cur_player_pos_len, PROT_WRITE, MAP_SHARED, *cur_player_pos_fd, 0))
       == MAP_FAILED)
  {
    perror("WARNING: failed to mmap current player position");
    return;
  }
  if (write(*cur_player_pos_fd, (void *)cur_player_pos, cur_player_pos_len) < 0)
  {
    perror("WARNING: cannot write to current player position file");
    return;
  }
  memcpy(*cur_player_pos_shared, (void *)cur_player_pos, cur_player_pos_len);
}
void record_cur_player_pos(void *cur_player_pos_shared, double *cur_player_pos, size_t cur_player_pos_len)
{
  // write to the shared variable
  memcpy(cur_player_pos_shared, (void *) cur_player_pos, cur_player_pos_len);
}
void close_cur_player_pos_file(int cur_player_pos_fd, void *cur_player_pos_shared, size_t cur_player_pos_len)
{
  // close the shared memory
  munmap(cur_player_pos_shared, cur_player_pos_len);
  close(cur_player_pos_fd);
}
