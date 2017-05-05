// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#include <stdio.h>
#include <string.h>
#include "log_params.h"
#include "gbsession.h"
#include "color.h"
#include "config.h"
#include "recovery.h"
void create_param_file(FILE **param_file)
{
  *param_file = fopen(GB_PARAMS_FILEPATH, "w");
}
void write_args_to_file(gb_t *gb, char *raw_args) 
{
  FILE *param_file = NULL;
  conf_t *conf = gb->conf;
  create_param_file(&param_file);
  if (param_file == NULL)
  {
    fprintf(stderr, "%c[%d;%d;%dmWARNING: cannot write param file!\n%c[%d;%dm\n", 
            0x1B, BRIGHT, RED, BG_BLACK, 0x1B, WHITE, ATTR_OFF);
    return;
  }
  // TODO make num_of_segments and segment size to NA_VALUE when use variable segmentation
  // TODO add initial segment size
  fprintf(param_file, "%s: %d\n", FIXED_SEGMENT_MODE, conf->fixed_segment_mode);
  fprintf(param_file, "%s: %d\n", FIXED_SUBSEGMENT_MODE, conf->fixed_subsegment_mode);
  fprintf(param_file, "%s: %d\n", ENERGY_SAVING_MODE, conf->minimum_energy_mode);
  fprintf(param_file, "%s: %d\n", QOS_KBPS, (conf->minimum_energy_mode) ? conf->qos_kbps : NA_VALUE);
  fprintf(param_file, "%s: %d\n", NUM_SEGMENTS, (conf->fixed_segment_mode) ? conf->num_of_segments : NA_VALUE);
  fprintf(param_file, "%s: %lld\n", SEGMENT_SIZE_KB, 
          (conf->fixed_segment_mode) ? conf->fixed_segment_size_KB : NA_VALUE);
  fprintf(param_file, "%s: %.2f\n", 
          INITIAL_SUBSEGMENT_RATE, 
          (float) conf->initial_chunk_rate[INDEX_LTE] / (float) conf->initial_chunk_rate[INDEX_WIFI]);
  fprintf(param_file, "%s: %lld\n", INITIAL_SEGMENT_SIZE_KB, gb->offset_info.init_seg_size / 1024);
  fprintf(param_file, "%s: %.2f\n", EWMA_FACTOR, conf->ewma_factor);
  fprintf(param_file, "%s: %.2f\n", BW_FACTOR_FOR_MODE, conf->BW_factor_for_mode);
  fprintf(param_file, "%s: %.2f\n", BW_FACTOR_FOR_STAGE, conf->BW_factor_for_stage);
  fprintf(param_file, "%s: %d\n", RECOVERY_MODE, conf->recovery_mode);
  fprintf(param_file, "%s: %.2f\n", RECOVERY_DECISION_FACTOR, conf->recovery_decision_factor);
  fprintf(param_file, "%s: %s\n", RAW_ARGUMENTS, raw_args);
  //fprintf(param_file, "%s: %d\n", NUM_CONNECTIONS_ARG, conf->num_connections);
  fclose(param_file);
}
void record_post_download_params(gb_t *gb, double finish_time)
{
  double download_time = finish_time - gb->start_time;
  FILE *param_file = NULL;
  if ((param_file = fopen(GB_PARAMS_FILEPATH, "a+")) == NULL)
  {
    fprintf(stderr, "%c[%d;%d;%dmWARNING: cannot append param file:%s\n%c[%d;%dm\n", 
            0x1B, BRIGHT, RED, BG_BLACK, strerror(errno), 0x1B, WHITE, ATTR_OFF);
    return;
  }
  fprintf(param_file, "%s: %.2f\n", FILE_DOWNLOAD_TIME_SEC, download_time);
  fprintf(param_file, "%s: %d\n", NUM_RECOVERIES, gb->num_recoveries);
  fclose(param_file);
}
