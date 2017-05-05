// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#include <stdio.h>
#include <assert.h>
#include "player_time.h"
#include "energy.h"
#include "recovery.h"
#include "gb.h"
#include "util.h"

#define HEADROOM_SINGLE_DUAL_FACTOR 0.9
#define HEADROOM_SINGLE_DUAL 0.0 //second
#define HEADROOM_DUAL_SINGLE_FACTOR 0.8 // 0.8 bandwidth est, 0.9 remaining playback time
#define HEADROOM_DUAL_SINGLE 0.0 //second
#define BN_QOS_MISS_HR_FACTOR 1.0
#define HEADROOM_TIME_REMAIN_DUAL 2.0 //sec
#define DUAL_LINK_BAG_EFF 0.8
#define HEADROOM_BN_FACTOR 0.9

#ifdef OS_ANDROID
#define MAX_NUM_EST_QMISS 3
#define MAX_NUM_GLITCH 2
#else
#define MAX_NUM_EST_QMISS 2
#endif

long long int estimate_link_mode_goodput(gb_t *gb, int link_mode);

/* 
 * next_portion_size can be the next segment size of remaining file size
 * or next segment size
 */
int choose_link_mode(gb_t *gb, int conn_index, long long int next_segment_size)
{
  static int current_link_mode = DUAL_LINK_MODE;
  int i = 0;
  int energy_sorted_link_modes[NUM_LINK_MODE] = {};
  long long int remaining_file_size = get_remaining_file_size(gb);
  static int num_est_qmiss = 0;
#ifdef OS_ANDROID
  static int num_glitch = 0;
#endif
 
  //printf("remaining_file_size: %lld\n", remaining_file_size);
  if (remaining_file_size <= 0)// finishing the last segment
    return current_link_mode;

  if (get_player_state() == PLAYER_PLAYING) { 
#ifdef OS_ANDROID
    num_glitch = 0;
#endif
    if ((current_link_mode == LTE_SINGLE_LINK_MODE) ||
        (current_link_mode == WIFI_SINGLE_LINK_MODE)) { // is in single link mode, estimate on next segment
      fprintf(stderr, "Is in single link mode\n");
      //if (does_qmiss_until_next_portion_in_link_mode(gb, next_segment_size, current_link_mode))
      if (does_qmiss_until_next_portion_in_link_mode(gb, remaining_file_size, current_link_mode)) {
        ++num_est_qmiss;
        if (num_est_qmiss >= MAX_NUM_EST_QMISS) {
          num_est_qmiss = 0;
          current_link_mode = DUAL_LINK_MODE;
        }
      } else {
        num_est_qmiss = 0;
        printf("Consider more energy efficient link mode\n");
        get_energy_sorted_link_modes(gb, conn_index, remaining_file_size, energy_sorted_link_modes);
        for (i = 0; i < NUM_LINK_MODE; ++i) {
          int link_mode = energy_sorted_link_modes[i];
          if (link_mode != DUAL_LINK_MODE) {
            if (!does_qmiss_until_next_portion_in_link_mode(gb, remaining_file_size, link_mode))
              current_link_mode = link_mode;
            break;
          } else {
            current_link_mode = DUAL_LINK_MODE; //redundant?
            break;
          }
        }
      }
    } else { // is in dual link mode, estimate on remaining file size
      if (!does_qos_miss_until_bn_chunk(gb, conn_index)) { // this should be the same decision with recovery ?
        get_energy_sorted_link_modes(gb, conn_index, remaining_file_size, energy_sorted_link_modes);
        for (i = 0; i < NUM_LINK_MODE; ++i) {
          int link_mode = energy_sorted_link_modes[i];
          if (link_mode != DUAL_LINK_MODE) {
            if (!does_qmiss_until_next_portion_in_link_mode(gb, remaining_file_size, link_mode))
              current_link_mode = link_mode;
            break;
          } else {
            current_link_mode = DUAL_LINK_MODE; //redundant?
            break;
          }
        }
      }
    }
  } else { // player not playing
#ifdef OS_ANDROID
    ++num_glitch;
    printf("glitch number %d\n", num_glitch);
    if (num_glitch >= MAX_NUM_GLITCH) {
      num_glitch = 0;
      current_link_mode = DUAL_LINK_MODE;
    }
#else
    current_link_mode = DUAL_LINK_MODE;
#endif
  }
  if (current_link_mode != DUAL_LINK_MODE)
    gb->offset_info.single_link_mode_flag = 1;
  else
    gb->offset_info.single_link_mode_flag = 0;
  printf("chosen link mode: %d\n", current_link_mode);
  return current_link_mode;
}

double download_time_next_portion_link_mode(gb_t *gb, long long int next_portion_size, int link_mode)
{
  return (double) next_portion_size / estimate_link_mode_goodput(gb, link_mode);
}

int does_qmiss_until_the_end_of_file_in_dual_link_mode(gb_t *gb)
{
  double file_duration = (double) gb->size / gb->qos_bytes_per_second;
  double remaining_qos_time = file_duration - get_cur_player_pos();
  long long int dual_link_mode_goodput = estimate_link_mode_goodput(gb, DUAL_LINK_MODE);
  double remaining_file_download_time_dual_link =
    (double) get_remaining_file_size(gb) / dual_link_mode_goodput;
  //printf("\nFile duration: %.3lf\n", file_duration);
  //printf("Remaining file size: %lld\tDual link mode goodput: %lld\n", get_remaining_file_size(gb), dual_link_mode_goodput);
  printf("Remaining file download time in dual link: %.3lf\tRemaining qos time: %.3lf\n",
          remaining_file_download_time_dual_link, remaining_qos_time);
  return (remaining_file_download_time_dual_link > (remaining_qos_time - HEADROOM_TIME_REMAIN_DUAL));
}

int does_qmiss_until_next_portion_in_link_mode(gb_t *gb, long long int next_portion_size, int link_mode)
{
  long long int goodput_next_portion = estimate_link_mode_goodput(gb, link_mode); 
  double next_portion_download_time = (double) next_portion_size / goodput_next_portion;
  double remaining_playback_time = get_remaining_playback_time(gb);

  printf("\nqmiss until next portion using link mode %d:\n", link_mode);
  printf("  Next portion size: %lld,%lld\t", next_portion_size / ONE_MIL, next_portion_size % ONE_MIL);
  printf("  Estimated goodput in next portion: %lld\n", goodput_next_portion); 
  printf("  Next portion download time: %.2lf\t", next_portion_download_time); 
  printf("  Remaining playback time: %.2f\n", remaining_playback_time);

  if (gb->offset_info.single_link_mode_flag)
    return (next_portion_download_time > HEADROOM_SINGLE_DUAL_FACTOR * remaining_playback_time + HEADROOM_SINGLE_DUAL);
  else // is in dual-link mode
    return (next_portion_download_time > HEADROOM_DUAL_SINGLE_FACTOR * remaining_playback_time + HEADROOM_DUAL_SINGLE);
}
/*
 * Estimate goodput in case of dual link mode is the total of two goodput
 */
int does_qmiss_until_next_portion_in_link_mode_old(gb_t *gb, long long int next_portion_size, int link_mode)
{
  long long int start_byte_next_segment = get_start_byte_next_segment(&gb->offset_info);
  long long int goodput_next_portion = estimate_link_mode_goodput(gb, link_mode); 
  double next_portion_download_time = (double) next_portion_size / goodput_next_portion;
  double qos_deadline = 0.0;
  long long int position_diff = start_byte_next_segment - estimate_cur_player_pos_offset(gb);
  long long int qos_conn_speed_diff = gb->qos_bytes_per_second - goodput_next_portion;
  if (next_portion_size < 0) //finished downloading
    return 0;

  if (qos_conn_speed_diff > 0)
    qos_deadline = (double) position_diff / qos_conn_speed_diff;
  else
    qos_deadline = INFINITE_TIME;
  printf("\nqmiss until next portion using link mode %d:\n", link_mode);
  printf("  Next portion size: %lld,%lld\t", next_portion_size / ONE_MIL, next_portion_size % ONE_MIL);
  printf("  Estimated goodput in next portion: %lld\n", goodput_next_portion); 
  printf("  Start byte of next segment: %lld\tCurrent player position offset: %lld\n",
      start_byte_next_segment, estimate_cur_player_pos_offset(gb));
  printf("  last_downloaded_byte: %lld\n", gb->last_downloaded_byte);
  printf("  Next portion download time: %.3lf\tqos deadline: %.3lf\n", next_portion_download_time, qos_deadline);
  if (gb->offset_info.single_link_mode_flag)
    return (next_portion_download_time > HEADROOM_SINGLE_DUAL_FACTOR * qos_deadline);
  else // is in dual-link mode
    return (next_portion_download_time > HEADROOM_DUAL_SINGLE_FACTOR * qos_deadline); // FIXME or <= 
}

int does_qos_miss_until_bn_chunk(gb_t *gb, int cur_fin_conn_index)
{
  chunk_t *bn_chunk = NULL;
  long long int remaining_bn_chunk_size = 0;
  int bn_conn_index = 0;
  long long int bottle_neck_size = 0;
  double bottle_neck_downloading_time = 0.0;
  double remaining_playable_time = 0.0;

  get_bn_chunk_info(gb, cur_fin_conn_index, &bn_chunk, &remaining_bn_chunk_size, &bn_conn_index);
  bottle_neck_size = remaining_bn_chunk_size;
  bottle_neck_downloading_time = (double) bottle_neck_size / gb->conn[bn_conn_index].bytes_per_second;
  remaining_playable_time = get_remaining_playable_time(gb);

  printf("bottleneck size: %lld, bottleneck downloading time(s): %.2f, remaining playable time: %.2f\n",
          bottle_neck_size, bottle_neck_downloading_time, remaining_playable_time);
  printf("bottleneck qos miss headroom factor: %.2f\n", BN_QOS_MISS_HR_FACTOR);

  return (remaining_playable_time < BN_QOS_MISS_HR_FACTOR * bottle_neck_downloading_time + 0.5);
}

int does_qos_miss_until_bn_chunk_old(gb_t *gb, int cur_fin_conn_index)
{
  chunk_t *bn_chunk = NULL;
  long long int remaining_bn_chunk_size = 0;
  int bn_conn_index = 0;
  get_bn_chunk_info(gb, cur_fin_conn_index, &bn_chunk, &remaining_bn_chunk_size, &bn_conn_index);
  double bn_chunk_download_time = (double) remaining_bn_chunk_size / gb->conn[bn_conn_index].bytes_per_second;

  long long int bn_bytes_difference =
    (bn_chunk->last_byte - remaining_bn_chunk_size) - estimate_cur_player_pos_offset(gb);
  if (bn_bytes_difference < 0)
    return 1;

  int qos_conn_speed_diff = gb->qos_bytes_per_second - gb->conn[bn_conn_index].bytes_per_second;
  double qos_deadline = INFINITE_TIME;
  if (qos_conn_speed_diff > 0)
    qos_deadline = (double) bn_bytes_difference / qos_conn_speed_diff;
  else
    qos_deadline = INFINITE_TIME;
#ifndef NDEBUG
  fprintf(stderr, "\nbn chunk current byte %lld\n", bn_chunk->last_byte - remaining_bn_chunk_size);
  fprintf(stderr, "estimated cur player pos offset: %lld\n", estimate_cur_player_pos_offset(gb));
  fprintf(stderr, "bn bytes difference: %lld\n", bn_bytes_difference);
  fprintf(stderr, "qos speed: %lld\tbn conn speed: %lld\n", gb->qos_bytes_per_second, gb->conn[bn_conn_index].bytes_per_second);
  fprintf(stderr, "bn chunk download time: %.3lf\tqos deadline: %.3lf\n", bn_chunk_download_time, qos_deadline);
  //fprintf(stderr, "last_downloaded_byte: %lld\tinorder offset: %lld\n", gb->last_downloaded_byte, gb->inorder_offset);
#endif
  return ((bn_chunk_download_time < HEADROOM_BN_FACTOR * qos_deadline) ? 0 : 1);
}

long long int estimate_link_mode_goodput(gb_t *gb, int link_mode)
{
  long long int goodput_next_seg = 0;
  switch (link_mode) {
    case LTE_SINGLE_LINK_MODE:
      goodput_next_seg = gb->conn[INDEX_LTE].ewma_bytes_per_second;
      break;
    case WIFI_SINGLE_LINK_MODE:
      goodput_next_seg = gb->conn[INDEX_WIFI].ewma_bytes_per_second;
      break;
    case DUAL_LINK_MODE:
      goodput_next_seg = 
        DUAL_LINK_BAG_EFF * (gb->conn[INDEX_LTE].ewma_bytes_per_second + gb->conn[INDEX_WIFI].ewma_bytes_per_second);
      break;
    default:
      assert(0); //unknown link mode
  }
  return goodput_next_seg;
}

int update_lte_state(gb_t *gb, double *out_tail_state_start_time)
{
  static int lte_state = LTE_IDLE_STATE;
  static double tail_state_start_time = 0.0;
  double now = gettime();
  int lte_current_goodput = gb->conn[INDEX_LTE].bytes_per_second;
  if (lte_state == LTE_IDLE_STATE) {
    if (lte_current_goodput > 0)
      lte_state = LTE_ACTIVE_STATE;
  } else if (lte_state == LTE_TAIL_STATE) {
    if (lte_current_goodput > 0)
      lte_state = LTE_ACTIVE_STATE;
    else if (now - tail_state_start_time > LTE_TAIL_STATE_TIMEOUT_SEC)
      lte_state = LTE_IDLE_STATE;
  } else if (lte_state == LTE_ACTIVE_STATE) {
    if (lte_current_goodput == 0) {
      lte_state = LTE_TAIL_STATE;
      tail_state_start_time = now;
    }
  }
  // Output values
  if (out_tail_state_start_time)
    *out_tail_state_start_time = tail_state_start_time;
  return lte_state;
}

#define LTE_POW_ALPHA 16.72
#define LTE_POW_BETA 2022.2
#define WIFI_POW_ALPHA 24.19
#define WIFI_POW_BETA 360.9
#define LTE_TAIL_POW 1350.0
// TODO: throughput = 1.2 goodput
// link_goodput: mbps
double get_energy_consumption(gb_t *gb, int cur_fin_conn_index,
                              int link_mode, long long int estimated_lte_goodput, long long int estimated_wifi_goodput, long long int data_size)
{
  double tail_start_time = 0.0;
  int current_lte_state = update_lte_state(gb, &tail_start_time);
  long long int dual_link_mode_estimated_subsegments[2] = {0};
  double remaining_lte_tail_sec = 0.0; // after LTE is disconnected
  double energy_consumption = 0.0;
  double est_lte_goodput_mbps = (double) estimated_lte_goodput / ONE_MIL;
  double est_wifi_goodput_mbps = (double) estimated_wifi_goodput / ONE_MIL;
  long long int virtual_seg_size = 0;

  assert(estimated_lte_goodput || estimated_wifi_goodput);

  if (data_size > 0) {
    switch (link_mode) {
      case DUAL_LINK_MODE:
        dual_link_mode_estimated_subsegments[INDEX_LTE] =
          estimated_lte_goodput * data_size / (estimated_lte_goodput + estimated_wifi_goodput);
        dual_link_mode_estimated_subsegments[INDEX_WIFI] = data_size - dual_link_mode_estimated_subsegments[INDEX_LTE];
        energy_consumption = (double) (LTE_POW_ALPHA * est_lte_goodput_mbps + LTE_POW_BETA) * // power_cons * time
                                  dual_link_mode_estimated_subsegments[INDEX_LTE] / estimated_lte_goodput; // keep unit correct!
        energy_consumption += (double) (WIFI_POW_ALPHA * est_wifi_goodput_mbps + WIFI_POW_BETA) *
                                  dual_link_mode_estimated_subsegments[INDEX_WIFI] / estimated_wifi_goodput;
#ifndef NDEBUG
        printf("now: %.2lf(s)\n", gettime() - gb->start_time);
        printf("    remaining data size: %lld,%.6lld\n", data_size/ ONE_MIL, data_size % ONE_MIL);
        printf("    estimated goodput:\n");
        printf("    - 0: %lld\n", estimated_lte_goodput);
        printf("    - 1: %lld\n", estimated_wifi_goodput);
        printf("    estimated subsegment:\n");
        printf("    - 0: %lld,%.6lld\n", dual_link_mode_estimated_subsegments[0] / ONE_MIL, dual_link_mode_estimated_subsegments[0] % ONE_MIL);
        printf("    - 1: %lld,%.6lld\n", dual_link_mode_estimated_subsegments[1] / ONE_MIL, dual_link_mode_estimated_subsegments[1] % ONE_MIL);
#endif
        break;
      case LTE_SINGLE_LINK_MODE:
        energy_consumption = (double) (LTE_POW_ALPHA * est_lte_goodput_mbps + LTE_POW_BETA) * data_size / estimated_lte_goodput;
        break;
      case WIFI_SINGLE_LINK_MODE:
        energy_consumption = (double) (WIFI_POW_ALPHA * est_wifi_goodput_mbps + WIFI_POW_BETA) * data_size / estimated_wifi_goodput;
        break;
      default:
        fprintf(stderr, "Unknown link mode\n");
        assert(0);
    }
    switch (current_lte_state) {
      case LTE_ACTIVE_STATE:
        remaining_lte_tail_sec = LTE_TAIL_STATE_TIMEOUT_SEC;
        break;
      case LTE_TAIL_STATE:
        remaining_lte_tail_sec = gettime() - tail_start_time;
        break;
      case LTE_IDLE_STATE:
        remaining_lte_tail_sec = 0.0;
        break;
      default:
        fprintf(stderr, "Unknown lte state\n");
        assert(0);
    }
    energy_consumption += LTE_TAIL_POW * remaining_lte_tail_sec;
  }
  return energy_consumption;
}

int get_min_value_index_double_array(double *array, int array_size)
{
  int i = 0;
  int min_value_index = 0;
  for (i = 1; i < array_size; ++i)
    if (array[i] < array[min_value_index])
      min_value_index = i;
  return min_value_index;
}
int get_max_value_index_double_array(double *array, int array_size)
{
  int i = 0;
  int max_value_index = 0;
  for (i = 1; i < array_size; ++i)
    if (array[i] > array[max_value_index])
      max_value_index = i;
  return max_value_index;
}
void get_three_sorted_index(double energy_consumptions[3], int energy_sorted_link_modes[3])
{
  int min_energy_link_mode = get_min_value_index_double_array(energy_consumptions, 3);
  int max_energy_link_mode = get_max_value_index_double_array(energy_consumptions, 3);
  energy_sorted_link_modes[0] = min_energy_link_mode;
  energy_sorted_link_modes[2] = max_energy_link_mode;
  int middle_energy_link_mode;
  for (middle_energy_link_mode = 0; middle_energy_link_mode < 3; ++middle_energy_link_mode) {
    if ((middle_energy_link_mode != min_energy_link_mode) && (middle_energy_link_mode != max_energy_link_mode)) {
      energy_sorted_link_modes[1] = middle_energy_link_mode;
      return;
    }
  }
}
/* 
 * out a list of link mode sorted by increasing energy consumption
 * TODO use enumaration type for LINK MODE and size of enumeration
 */
void get_energy_sorted_link_modes(gb_t *gb, int cur_fin_conn_index, long long int next_seg_size, int energy_sorted_link_modes[3])
{
  double energy_consumptions[NUM_LINK_MODE] = {0.0};
  long long int estimated_goodput[2] = {0};

  estimated_goodput[INDEX_LTE] = estimate_link_mode_goodput(gb, LTE_SINGLE_LINK_MODE);
  estimated_goodput[INDEX_WIFI] = estimate_link_mode_goodput(gb, WIFI_SINGLE_LINK_MODE);

  // DUAL_LINK_MODE energy
  energy_consumptions[DUAL_LINK_MODE] = 
    //get_energy_consumption(gb, cur_fin_conn_index, DUAL_LINK_MODE, estimated_goodput[INDEX_LTE], estimated_goodput[INDEX_WIFI], next_seg_size);
    get_energy_consumption(gb, cur_fin_conn_index, DUAL_LINK_MODE, estimated_goodput[INDEX_LTE], estimated_goodput[INDEX_WIFI], next_seg_size);
  // LTE_SINGLE_LINK_MODE
  energy_consumptions[LTE_SINGLE_LINK_MODE] =
    get_energy_consumption(gb, cur_fin_conn_index, LTE_SINGLE_LINK_MODE, estimated_goodput[INDEX_LTE], 0, next_seg_size);
  // WIFI_SINGLE_LINK_MODE
  energy_consumptions[WIFI_SINGLE_LINK_MODE] =
    get_energy_consumption(gb, cur_fin_conn_index, WIFI_SINGLE_LINK_MODE, 0, estimated_goodput[INDEX_WIFI], next_seg_size);

  // sorting the energy 
  assert(NUM_LINK_MODE == 3);
  get_three_sorted_index(energy_consumptions, energy_sorted_link_modes);

  int i;
  printf("Energy sorted link modes:\n");
  for (i = 0; i < NUM_LINK_MODE; ++i)
    printf("    %d: %.2lf(mJ)\n", energy_sorted_link_modes[i], energy_consumptions[energy_sorted_link_modes[i]]);
}
