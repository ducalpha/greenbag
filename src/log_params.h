// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#ifndef _LOG_PARAM_H_
#define _LOG_PARAM_H_
#include <stdio.h>
#include "gb.h"
#define NA_VALUE -1  // NA = not available: the value is meaningless with the setup
#define GB_PARAMS_FILEPATH "gb_params"
#define FIXED_SEGMENT_MODE "Fixed Segment Mode"
#define FIXED_SUBSEGMENT_MODE "Fixed Subsegment Mode"
#define ENERGY_SAVING_MODE "Energy Saving Mode"
#define QOS_KBPS "QoS(kbps)"
#define NUM_SEGMENTS "Number of Segments"
#define SEGMENT_SIZE_KB "Segment Size(KB)"
#define INITIAL_SUBSEGMENT_RATE "Initial Subsegment Rate"
#define INITIAL_SEGMENT_SIZE_KB "Initial Segment Size(KB)"
#define RECOVERY_MODE "Recovery Mode"
#define RECOVERY_DECISION_FACTOR "Recovery Decision Factor"
#define EWMA_FACTOR "EWMA Factor"
#define BW_FACTOR_FOR_MODE "BW Factor for Mode"
#define BW_FACTOR_FOR_STAGE "BW Factor for stage"
#define NUM_CONNECTIONS_ARG "Number of Connections"
#define RAW_ARGUMENTS "Raw Argument"
#define FILE_DOWNLOAD_TIME_SEC "File Download Time(sec)"
//#define NUM_RECOVERIES "Number of Recoveries"
#define NUM_RECOVERIES "Number of Recoveries"
void create_param_file(FILE **param_file);
void write_args_to_file(gb_t *gb, char *raw_args);
void record_post_download_params(gb_t *gb, double finish_time);
#endif //_LOG_PARAM_H_
