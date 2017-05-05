// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#ifndef _CONN_H_
#define _CONN_H_
/* Connection stuff							*/
#define PROTO_HTTP 1
#define PROTO_FTP  2
#define PROTO_DEFAULT PROTO_HTTP

typedef struct
{
	conf_t *conf;// points to the parent's conf
	int proto;
	int port;
	char host[MAX_STRING];
	char dir[MAX_STRING];
	char file[MAX_STRING];
	char user[MAX_STRING];
	char pass[MAX_STRING];
	http_t http[1];
	long long int size;		/* File size, not 'connection size'..	*/
	long long int currentbyte; /* last downloaded byte index + 1, e.g. initial, set at 1 */
	long long int lastbyte;
	int fd;
	int enabled;
	int supported;
	int last_transfer;
	char *message;
	int state; // Duc's comment: 0: not downloading, 1: downloading
	pthread_t setup_thread[1];
	char *local_if;

	long long int bytes_done;			// received bytes. It is used by calculating goodput.
	double start_time;					// start time for receiving bytes. It is used by calculating goodput.
	long long int bytes_per_second;				// instaneous goodput
	long long int ewma_bytes_per_second;				// exponentially weighted moving average goodput
	int pipeline_requests_sent;					// flag for sending next request. 1 means that next request is sent
	int last_segment_flag;				// If next request for last segment is sended, this flag is set to 1.
	double rtt;
	double rtt_dev;
} conn_t;

int conn_set( conn_t *conn, char *set_url );
char *conn_url( conn_t *conn );
void conn_disconnect( conn_t *conn );
int conn_init( conn_t *conn );
int conn_setup( conn_t *conn );
int conn_exec( conn_t *conn );
int conn_info( conn_t *conn );
#endif
