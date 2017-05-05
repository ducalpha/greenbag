// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#include "extra_info.h"
#include "gbsession.h"
#include "gb.h"
#include "util.h"
#include "algo_general.h"
#include "periodic_tasks.h"
#include "offset.h"
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <assert.h>
#include "log_params.h"
#include "util.h"
#include "color.h"
#define LINEWIDTH 80
gb_t *gb;
conf_t conf[1];
//timing_info_t timing;
double start_session_time = 0.0;
time_t start_session_time_t;
// info string
char string[MAX_STRING];
void print_finish_message(void);
void print_loading_bar(long long int prev_bytes_done);
void join_periodic_tasks(void);
int parse_args(conf_t *conf, int argc, char **argv);
void print_help(void);

// mode: download mode, maximize performance or minimze energy
// return NULL, however should return the downloaded file name
char *gb_process(const char *url, int mode, const struct extra_info *einfo, int gb_process_argc, char **gb_process_argv)
{
  long long int prev_bytes_done;
  int arg_index;
  char raw_args[1024] = "";
  start_session_time = gettime();
  time(&start_session_time_t);
  for (arg_index = 1; arg_index < gb_process_argc; ++arg_index)
  {
    strcat(raw_args, gb_process_argv[arg_index]);
    strcat(raw_args, " ");
  }

  if(!conf_init(conf))
      return NULL;
  if (parse_args(conf, gb_process_argc, gb_process_argv) != SUCCESS_STATUS)
    return NULL;

  gb = gb_new(conf, url, einfo);
  assert(gb->conf->num_connections <= 2);
  if(gb->ready == -1) {
    printf("Axel creation error: ");
    print_messages(gb);
    gb_close(gb);
    return NULL;
  }

  printf("Initializing download: %s\n", url);

  if(!gb_open(gb)) {
    error("gb_process: Error opening local file: ");
    print_messages(gb);
    exit(1);
  }

  // after calling gb_init_segment_setup() in gb_open()
  write_args_to_file(gb, raw_args);
  //gb->performAlgorithm = algo_general;
  printf("Starting periodic tasks");
  if (start_periodic_tasks(gb) != SUCCESS_STATUS) {
    printf("something wrong\n");
  }

  gb_start(gb);//axel_start
  update_last_downloaded_byte(gb);
  print_messages(gb);

  while(!gb->ready && gb->run) {
    prev_bytes_done = gb->bytes_done;
    if (!gb) fprintf(stderr, "gb is NULL\n");
    algo_general(gb); //gb->performAlgorithm(gb); 
    update_last_downloaded_byte(gb);
    //print_loading_bar(prev_bytes_done);
  }

  update_last_downloaded_byte(gb);
  fprintf(stderr, "End\n");
  join_periodic_tasks(); // block until gb->finish is set
  print_finish_message();
  gb_close(gb);   
  gb = NULL;
  return NULL;
}
void join_periodic_tasks(void)
{
  void *result;
#if ENABLE_UPDATE_RTT
  pthread_join(update_rtt_tid, NULL);
  printf("End send probe thread\n");
#endif
#if ENABLE_MEASURE_RTT
  pthread_join(measure_rtt_tid, NULL);
  printf("End measure rtt thread\n");
#endif
#if ENABLE_LOG_GOODPUT
  pthread_join(log_goodput_tid, NULL);
  printf("End log goodput thread\n");
#endif
#if ENABLE_LOG_GOODPUT_TO_CLIENT
  pthread_join(log_goodput_to_client_tid, NULL);
  printf("End log goodput to client thread\n");
#endif
  /*fprintf(stderr, "Wait for log_fps finishes\n");
  pthread_join(log_fps_tid, &result);
  timing = *(timing_info_t *)result;*/
  //pthread_join(initiate_periodic_tasks_tid, NULL); // this will finish when gb is NULL
}

void print_loading_bar(long long int prev_bytes_done)
{
  if((!gb->message) && (prev_bytes_done != gb->bytes_done))
    print_alternate_output(gb);
  if(gb->message) {
   clrline();
   print_messages(gb);
    if(!gb->ready)
      print_alternate_output(gb);
  } else if(gb->ready) {
    putchar('\n');
  }
}
void print_finish_message(void)
{
  double finish_time = gettime();
  strcpy(string + MAX_STRING / 2, size_human(gb->bytes_done - gb->start_byte)); //bad code? size_human already set string to megabytes, but this line also duplicate it to the middle of variable string; now string is ("256.0 megabytes", '\000' <repeats 497 times>, "256.0 megabytes", '\000' <repeats 496 times>)
  printf("%c[%d;%d;%dm", 0x1B, NORMAL, CYAN, BG_BLACK);
  printf("\nDownloaded %s in %s. (%.2f KB/s)\n", string + MAX_STRING / 2, time_human(finish_time - gb->start_time), (double) gb->bytes_per_second / 1024);
  printf("%c[%d;%dm\n", 0x1B, WHITE, ATTR_OFF);
  record_post_download_params(gb, finish_time);
}

// clear all the line, the cursor does not move
void clrline()
{
  int i;
  putchar('\r');
  for(i=0; i < LINEWIDTH; ++i)
    putchar(' ');
  putchar('\r');
}
/* Print any message in the gb structure				*/
void print_messages( gb_t *gb )
{
	message_t *m;
	
	while( gb->message ) {
		printf( "%s\n", gb->message->text );
		m = gb->message;
		gb->message = gb->message->next;
		free( m );
	}
}
void error(char *str)
{
  fprintf(stderr, "%s", str);
}
void info(int i)
{
  if(i)
    fprintf(stderr, "Info %d: ", i);
  else 
    fprintf(stderr, "Info: ");
}

/* Convert a number of bytes to a human-readable form			*/
char *size_human( long long int value )
{
	if( value == 1 )
		sprintf( string, _("%lld byte"), value );
	else if( value < 1024 )
		sprintf( string, _("%lld bytes"), value );
	else if( value < 10485760 )
		sprintf( string, _("%.1f kilobytes"), (float) value / 1024 );
	else
		sprintf( string, _("%.1f megabytes"), (float) value / 1048576 );
	
	return( string );
}

/* Convert a number of seconds to a human-readable form			*/
char *time_human( int value )
{
	if( value == 1 )
		sprintf( string, _("%i second"), value );
	else if( value < 60 )
		sprintf( string, _("%i seconds"), value );
	else if( value < 3600 )
		sprintf( string, _("%i:%02i seconds"), value / 60, value % 60 );
	else
		sprintf( string, _("%i:%02i:%02i seconds"), value / 3600, ( value / 60 ) % 60, value % 60 );
	
	return( string );
}

void print_alternate_output(gb_t *gb) 
{
	long long int done=gb->bytes_done;
	long long int total=gb->size;
	int i,j=0;
	double now = gettime();
	printf("\r[%3ld%%] [", min(100,(long)(done*100./total+.5) ) );
	for(i=0;i<gb->conf->num_connections;i++)
	{
		for(;j<((double)gb->conn[i].currentbyte/(total+1)*50)-1;j++)
			putchar('.');
		if(gb->conn[i].currentbyte<gb->conn[i].lastbyte)
		{
			if(now <= gb->conn[i].last_transfer + gb->conf->connection_timeout/2 )
				putchar(i+'0');
			else
				putchar('#');
		} else 
			putchar('.');
		j++;
		for(;j<((double)gb->conn[i].lastbyte/(total+1)*50);j++)
			putchar(' ');
	}
	if(gb->bytes_per_second > 1048576)
		printf( "] [%6.1fMB/s]", (double) gb->bytes_per_second / (1024*1024) );
	else if(gb->bytes_per_second > 1024)
		printf( "] [%6.1fKB/s]", (double) gb->bytes_per_second / 1024 );
	else
		printf( "] [%6.1fB/s]", (double) gb->bytes_per_second );
	if(done<total)
	{
		int seconds,minutes,hours,days;
		seconds=gb->finish_time - now;
		minutes=seconds/60;seconds-=minutes*60;
		hours=minutes/60;minutes-=hours*60;
		days=hours/24;hours-=days*24;
		if(days)
			printf(" [%2dd%2d]",days,hours);
		else if(hours)
			printf(" [%2dh%02d]",hours,minutes);
		else
			printf(" [%02d:%02d]",minutes,seconds);
	}
	fflush( stdout );
}

/*#ifdef NOGETOPTLONG
#define getopt_long( a, b, c, d, e ) getopt( a, b, c )
#else*/
static struct option gb_options[] =
{
	/* name			has_arg	flag	val */
	{ "num-connections",	1,	NULL,	'n' },
	{ "verbose",		0,	NULL,	'v' },
//sangeun
	{ "num_of_segments", 1, NULL, 'i'}, 
	{ "link_1_chunk_rate", 1, NULL, '1'},
	{ "link_2_chunk_rate", 1, NULL, '2'},
	{ "Required-QoS", 1, NULL, 'Q'},
	{ "Fixed-stage-size-mode", 1, NULL, 'F'},
	{ "Fixed-segment-size-mode", 1, NULL, 'f'},
	{ "Minmum-energy-mode", 1, NULL, 'E'},
	{ "Weighted-Bandwidth-factor", 1, NULL, 'W'},
	{ "Bandwidth-factor-for-mode", 1, NULL, 'A'},
	{ "Bandwidth-factor-for-stage", 1, NULL, 'B'},
	{ "Log-file-name", 1, NULL, 'l'},
	{ "Fixed segment size", 1, NULL, 'S'},
	{ "Recovery mode", 1, NULL, 'R'},
	{ "Recovery decision factor", 1, NULL, 'd'},
//sangeun
	{ NULL,			0,	NULL,	0 }
};
/*#endif*/

int parse_args(conf_t *conf, int argc, char **argv)
{
	int option;
	int j = -1;
	opterr = 0;
  // reset optind
  optind = 1;
  int i_opt_set = 0;
  int S_opt_set = 0;
  //printf("optind:%d opterr:%d optopt:%d\n",optind,opterr,optopt);
	while ((option = getopt_long(argc, argv, "n:vi:1:2:Q:F:f:E:W:A:B:l:S:R:d:", gb_options, NULL )) != -1)
  {
#ifndef NDEBUG
    fprintf(stderr, "parse gbprocess's option: %c\n", option);
    fprintf(stderr, "optarg: %s, optind: %d, argv[optind]: %s\n", optarg, optind, argv[optind]);
#endif
		switch( option )
		{
		case 'n':
			if( !sscanf( optarg, "%i", &conf->num_connections ) )
			{
				print_help();
				return FAILURE_STATUS;
			}
			break;
		case 'v':
			if( j == -1 )
				j = 1;
			else
				j ++;
			break;
//sangeun			
		case 'i':
			if( !sscanf( optarg, "%i", &conf->num_of_segments ) )
			{
				print_help();
				return FAILURE_STATUS;
			}
      i_opt_set = 1;
			break;
		case 'S':
			if( !sscanf( optarg, "%lld", &conf->fixed_segment_size_KB ) )
			{
				print_help();
				return FAILURE_STATUS;
			}
      S_opt_set = 1;
			break;
		case '1':
			if( !sscanf( optarg, "%i", &conf->initial_chunk_rate[0] ) )
			{
				print_help();
				return FAILURE_STATUS;
			}
			break;
		case '2':
			if( !sscanf( optarg, "%i", &conf->initial_chunk_rate[1] ) )
			{
				print_help();
				return FAILURE_STATUS;
			}
			break;
		case 'Q':
			if( !sscanf( optarg, "%d", &conf->qos_kbps ) )
			{
				print_help();
				return FAILURE_STATUS;
			}
			break;
		case 'F':
			if( !sscanf( optarg, "%i", &conf->fixed_segment_mode ) )
			{
				print_help();
				return FAILURE_STATUS;
			}
			break;
		case 'f':
			if( !sscanf( optarg, "%i", &conf->fixed_subsegment_mode ) )
			{
				print_help();
				return FAILURE_STATUS;
			}
			break;
		case 'E':
			if( !sscanf( optarg, "%i", &conf->minimum_energy_mode ) )
			{
				print_help();
				return FAILURE_STATUS;
			}
			break;
		case 'W':
			if( !sscanf( optarg, "%lf", &conf->ewma_factor ) )
			{
				print_help();
				return FAILURE_STATUS;
			}
			break;
		case 'A':
			if( !sscanf( optarg, "%lf", &conf->BW_factor_for_mode ) )
			{
				print_help();
				return FAILURE_STATUS;
			}
			break;
		case 'B':
			if( !sscanf( optarg, "%lf", &conf->BW_factor_for_stage ) )
			{
				print_help();
				return FAILURE_STATUS;
			}
			break;
		case 'l':
			strncpy( conf->log_file, optarg, MAX_STRING);
			break;
		case 'R':
			if( !sscanf( optarg, "%i", &conf->recovery_mode ) )
			{
				print_help();
				return FAILURE_STATUS;
			}
			break;
		case 'd':
			if( !sscanf( optarg, "%f", &conf->recovery_decision_factor ) )
			{
				print_help();
				return FAILURE_STATUS;
			}
			break;

		//sangeun
// ignore the unrecognized argument
//		default:
//			print_help();
//			return FAILURE_STATUS;
		}
	}
  if (i_opt_set && S_opt_set) {
    fprintf(stderr, "WARNING: both number of segment and segment size are set. Ignore number of segment option\n");
    conf->num_of_segments = 0;
  }

  return SUCCESS_STATUS;
}
void print_help(void)
{
#ifdef NOGETOPTLONG
	printf("Usage: gproxy [options]\n"
		"\n"
		"-s x\tSpecify maximum speed (bytes per second)\n"
		"-n x\tSpecify maximum number of connections\n"
		"-v\tMore status information\n"
		"\n");
#else
	printf("Usage: gproxy [options] url1 [url2] [url...]\n"
		"\n"
		"--max-speed=x\t\t-s x\tSpecify maximum speed (bytes per second)\n"
		"--num-connections=x\t-n x\tSpecify maximum number of connections\n"
		"--verbose\t\t-v\tMore status information\n"
		"\n");
#endif
}
