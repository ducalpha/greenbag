// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#ifndef _GB_H_
#define _GB_H_
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <netdb.h>
#ifndef	NOGETOPTLONG
#include <getopt.h>
#endif
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <pthread.h>
#include <assert.h>
#include "config.h"
#include "extra_info.h"

#define ONE_MIL 1000000

#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x),1)
#define unlikely(x)     __builtin_expect(!!(x),0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

#define min( a, b )		( (a) < (b) ? (a) : (b) )
#define max( a, b )		( (a) > (b) ? (a) : (b) )
//#define RATE_OF_CHUNK	2 --> @Sangeun: should this def unnecessary?
typedef struct
{
	void *next;
	char text[MAX_STRING];
} message_t;
typedef message_t url_t;
typedef message_t if_t;

// include after definition of if_t
#include "conf.h"
#include "http.h"
#include "conn.h"
struct gb_struct;
#include "offset.h"
typedef void (*gb_algorithm)(struct gb_struct *);
typedef struct gb_struct
{
	conn_t *conn;
	conf_t conf[1];
	char filename[MAX_STRING];
	double start_time;
	int finish_time;
	long long bytes_done, start_byte, size;
	int bytes_per_second;
	int delay_time;
	int outfd;
	int ready; // downloaded file is ready, i.e complete, ready = 1 --> complete, -1, error; 0, not ready
	message_t *message;
	url_t *url;
  int run;
  int finish; // finish gbprocess, finish transferring video file to the client,
            // set to 1 by ahttp only, or by gprofiler only
  /* Buffer file information */
  long long int last_downloaded_byte; //last in-order downloaded byte
	FILE *logfd;					
  FILE *log_recovery;

	long long int qos_bytes_per_second;		// bit rate of video (bytes per second)
	long long int qos_bytes;		// current position of video
	double qos_start_time;			// start time of video
	int qos_miss_flag;				// video interruption flag - 1 means interruption occurs
	int qos_start_flag;				// flag for starting video - 1 means video starts
	double buffering_start_time;	// start time of buffering(interruption)
	struct offset_info_struct offset_info;		// offset information
	int inorder_conn_id;			// connection index related to inorder byte
	long long int inorder_offset;	// inorder byte offset
  	// methods
 	gb_algorithm performAlgorithm;
  int num_recoveries;
} gb_t;

/* globally shared variables */
extern char *buffer;
/* functions from other modules */
/* gb-related functions */
gb_t *gb_new(conf_t *conf, const char *url, const struct extra_info *einfo);
int gb_open( gb_t *gb );
void gb_start( gb_t *gb );
void gb_close( gb_t *gb );
int gb_start_periodic_tasks(const gb_t *);
int gb_set_periodic_init_task(const gb_t *gb);
int gb_start_timer(const gb_t *);
void *setup_thread( void * );
void gb_message( gb_t *gb, char *format, ... );

int send_http_request_for_subsegment(conn_t *conn, subsegment_t *subseg_p);	// sending GET request for subsegment
void gb_get_request(conn_t *conn, long long int first_byte, long long int last_byte);	// sending get request
int gb_receive_http_header_response(http_t *conn);			// receiving the response for next request
void gb_init_segment_setup( gb_t *gb );				// initial segmenting & subsegmenting
void gb_update_goodput(gb_t *gb);	//update conn.bytes_per_second of both, offset_info.fast_conn
                                                  //reset bytes_done of both connections to 0
                                                  //reset start_time of both conns to now
int gb_setup_next_segment(gb_t *gb, int index);	    // next segmenting & subsegmenting
long long int gb_determine_next_segment_policy(gb_t *gb, int index, int energy_mode);	// policy of the segmenting
int gb_determine_next_segment(gb_t *gb, int index, int energy_mode);		// segmenting
long long int gb_determine_next_segment_size(gb_t *gb, int index); 
void gb_determine_next_subsegment(gb_t* gb, int index, int energy_mode);	// subsegmenting
int select_mode_for_energy(gb_t* gb);				// selecting performance mode or energy mode, set single_link_mode_flag
void gb_get_ip(char **addr_list);					// getting IP address
int update_inorder_offset(gb_t *gb);

void mark_qos_start_time(gb_t *gb);				// starting estimation of qos_bytes
int exception_for_read(gb_t *gb, long long int size, int index);		// exception handler of read()
void checking_last_subseg(gb_t *gb, int index); // checking whether the subsegment has last byte offset
void write_received_data(gb_t *gb, int index, long long int size);	// writing received data to file
void return_downloading_for_idle_conn(gb_t *gb, int index);	// receiving the response for next request, when a connection is idle
void finish_downloading_subseg(gb_t *gb, int index);	// processing routine after completing one subsegment
void update_qos_estimation(gb_t *gb);	// checking whether QoS is satisfied now
void check_conn(gb_t *gb);	// processing routine for aborted connections
void checking_last_segment(gb_t *gb);
void display_offset_info(gb_t *gb, int cur_fin_conn_index);
long long int get_remaining_file_size(gb_t *gb);
int is_using_only_one_link(gb_t *gb);
#endif
