// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#ifndef _CONF_H_
#define _CONF_H_
/* Configuration handling include file					*/
//#include "gb.h"
typedef struct
{
	char default_filename[MAX_STRING];
	int strip_cgi_parameters;
	int connection_timeout;
	int reconnect_delay;
	int num_connections;
	int buffer_size;
	int verbose;
	int alternate_output;
	if_t *interfaces;
	int search_timeout;
	int search_threads;
	int search_amount;
	int search_top;
	int add_header_count;
	char add_header[MAX_ADD_HEADERS][MAX_STRING]; /* a header added to HTTP request */
	char user_agent[MAX_STRING];

	int num_of_segments;
	int initial_chunk_rate[2];
	int qos_kbps;
	int fixed_segment_mode;
	int fixed_subsegment_mode;
	int minimum_energy_mode;
	double ewma_factor;
	double BW_factor_for_mode;
	double BW_factor_for_stage;
	char log_file[MAX_STRING];
	long long fixed_segment_size_KB;
	int recovery_mode;
	float recovery_decision_factor;
} conf_t;

int conf_loadfile( conf_t *conf, char *file );
int conf_init( conf_t *conf );
#endif
