// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#ifndef _GBSESSION_H_
#define _GBSESSION_H_

#include "extra_info.h"
#include "gb.h"

char *gb_process(const char *url, int mode, const struct extra_info *einfo, int gb_process_argc, char **gb_process_argv);
void clrline();
void error(char *str);
void info(int i);
void print_messages(gb_t *gb);
void stop(int signal);
char *size_human( long long int value );
char *time_human( int value );
void print_alternate_output( gb_t *gb );

#define BUFFER_SIZE 2048 // 2048 bytes
#define GB_MAX_PERFORMANCE 0
#define GB_SAVE_ENERGY 1
extern char *buf_filepath;
extern long long int last_downloaded_byte;
extern long long int buf_file_size;
extern int run;
extern gb_t *gb;

#endif
