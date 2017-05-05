// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#include "recovery.h"
#include "util.h"
#include "gbsession.h"
#include "offset.h"
#include "gb.h"
#include "player_time.h"
#include "energy.h"
#include <netinet/tcp.h>
#include <assert.h>
#define NDEBUG
#define LOG_OFFSET_WRITTEN 1
#define LOG_INORDER_OFFSET 0

int gb_stop_timer(const gb_t *gb);
void open_log_recovery(gb_t *gb);
void close_log_recovery(gb_t *gb);
void print_info_before_setup_next_segment(gb_t *gb, int conn_index);
void print_info_after_setup_next_segment(gb_t *gb, int conn_index);
void print_link_mode(int link_mode);
char *buffer = NULL;
void reset_bytes_done_start_time(gb_t *gb);
/* Create a new gb_t structure					*/
gb_t *gb_new(conf_t *conf, const char *url, const struct extra_info *einfo)
{
	gb_t *gb;
	char *s;
	
	gb = malloc( sizeof( gb_t ) );
	memset( gb, 0, sizeof( gb_t ) );
	*gb->conf = *conf;

	gb->conn = malloc( sizeof( conn_t ) * gb->conf->num_connections );
	memset( gb->conn, 0, sizeof( conn_t ) * gb->conf->num_connections );
	if( buffer == NULL )
		buffer = malloc( max( MAX_STRING, gb->conf->buffer_size ) );
	
  gb->url = malloc( sizeof( url_t ) );
	gb->url->next = gb->url;
	strncpy( gb->url->text, (char *) url, MAX_STRING );
	
	gb->conn[0].conf = gb->conf;
	if( !conn_set( &gb->conn[0], gb->url->text ) )
	{
		gb_message( gb, _("Could not parse URL.\n") );
		gb->ready = -1;
		return( gb );
	}

	gb->conn[0].local_if = gb->conf->interfaces->text;
	gb->conf->interfaces = gb->conf->interfaces->next;
	
	strncpy( gb->filename, gb->conn[0].file, MAX_STRING );
	http_decode( gb->filename );
	if( *gb->filename == 0 )	/* Index page == no fn		*/
		strncpy( gb->filename, gb->conf->default_filename, MAX_STRING );
	if( ( s = strchr( gb->filename, '?' ) ) != NULL && gb->conf->strip_cgi_parameters )
		*s = 0;		/* Get rid of CGI parameters		*/
	
	if( !conn_init( &gb->conn[0] ) )
	{
		gb_message( gb, gb->conn[0].message );
		gb->ready = -1;
		return( gb );
	}
	
	/* This does more than just checking the file size, it all depends
	   on the protocol used.					*/
	if( !conn_info( &gb->conn[0] ) )
	{
		gb_message( gb, gb->conn[0].message );
		gb->ready = -1;
		return( gb );
	}
	s = conn_url( gb->conn );
	strncpy( gb->url->text, s, MAX_STRING );
	if( ( gb->size = gb->conn[0].size ) != INT_MAX )
	{
		if( gb->conf->verbose > 0 )
		{
			gb_message( gb, _("File size: %lld bytes"), gb->size );
		}
	}
	
	/* Wildcards in URL --> Get complete filename			*/
	if( strchr( gb->filename, '*' ) || strchr( gb->filename, '?' ) )
		strncpy( gb->filename, gb->conn[0].file, MAX_STRING );
  long int url_bitrate = 0;
  /* get the true bit rate */
  //if (gb->conf->minimum_energy_mode)
    if ((url_bitrate = get_bitrate(url)) != -1) { // otherwise, go with default value of qos
      gb->conf->qos_kbps = url_bitrate;
      //printf("Got bitrate: %ld\n", url_bitrate);
    }

  gb->num_recoveries = 0;
  open_cur_player_pos();
  gb->run = 1;
  gb->finish = 0;
	
	return( gb );
}

/* Open a local file to store the downloaded data			*/
// returning 0 indicates error
int gb_open( gb_t *gb )
{
	long long int j;
	gb->outfd = -1;
	/* Check whether server knows about RESTart and switch back to
	   single connection download if necessary			*/
	if( !gb->conn[0].supported )
	{
		gb_message( gb, _("Server unsupported, "
			"starting from scratch with one connection.") );
		gb->conf->num_connections = 1;
		gb->conn = realloc( gb->conn, sizeof( conn_t ) );
		gb_init_segment_setup( gb );//@Sangeun: is this redundant?
	}
	/* If we have to start from scrath now		*/
	gb_init_segment_setup( gb ); 
  if( ( gb->outfd = open( gb->filename, O_CREAT | O_WRONLY, 0666 ) ) == -1 )
  {
    gb_message( gb, _("Error opening local file") );
    return( 0 );
  }
  /* And check whether the filesystem can handle seeks to
     past-EOF areas.. Speeds things up. :) AFAIK this
     should just not happen:				*/
  if( lseek( gb->outfd, gb->size, SEEK_SET ) == -1 && gb->conf->num_connections > 1 )
  {
    /* But if the OS/fs does not allow to seek behind
       EOF, we have to fill the file with zeroes before
       starting. Slow..				*/
    gb_message( gb, _("Crappy filesystem/OS.. Working around. :-(") );
    lseek( gb->outfd, 0, SEEK_SET );
    memset( buffer, 0, gb->conf->buffer_size );
    j = gb->size;
    while( j > 0 )
    {
      write( gb->outfd, buffer, min( j, gb->conf->buffer_size ) );
      j -= gb->conf->buffer_size;
    }
  }
	return( 1 );
}

/* Start downloading							*/
void gb_start( gb_t *gb )
{
	int i;
	char *addr_list[2];
	char log_file_name[MAX_STRING];

	for(i=0; i < 2; i++)
	{
		addr_list[i]=(char*)malloc(sizeof(char)*20);
	}

	gb_get_ip(addr_list);

	/* HTTP might've redirected and FTP handles wildcards, so
	   re-scan the URL for every conn				*/
	for( i = 0; i < gb->conf->num_connections; i ++ )
	{
		conn_set( &gb->conn[i], gb->url->text );
		gb->url = gb->url->next;
		gb->conn[i].local_if = gb->conf->interfaces->text;
		gb->conf->interfaces = gb->conf->interfaces->next;
		gb->conn[i].conf = gb->conf;
		if( i ) gb->conn[i].supported = 1;
	}
	
	if( gb->conf->verbose > 0 )
		gb_message( gb, _("Starting download") );
	
	for( i = gb->conf->num_connections - 1; i >= 0; i -- )
	if( gb->conn[i].currentbyte <= gb->conn[i].lastbyte )
	{
		gb->conn[i].state = 1;
		/*if(i == 0)
			gb->conn[i].local_if = addr_list[0];
		else if(i == 1)
			gb->conn[i].local_if = addr_list[1];*/
    char found_if[16] = "";
    char found_ip[24] = "";
    switch (i) {
      case INDEX_LTE:
        lookup_if_name(found_if, INDEX_LTE);
        break;
      case INDEX_WIFI:
        lookup_if_name(found_if, INDEX_WIFI);
        break;
      default:
        fprintf(stderr, "WARNING: unknown connection index");
			  gb->ready = -1;
        return;
    }
    get_if_ip(found_if, found_ip);
    gb->conn[i].local_if = strdup(found_ip);
		if( gb->conf->verbose >= 0 )
		{
			gb_message( gb, _("gb_start: Connection %i downloading from %s:%i using interface %s"),
		        	      i, gb->conn[i].host, gb->conn[i].port, gb->conn[i].local_if );
		}
#ifndef NDEBUG
		printf("IP : %s\n", gb->conn[i].local_if);
#endif	
		if( pthread_create( gb->conn[i].setup_thread, NULL, setup_thread, &gb->conn[i] ) != 0 )
		{
			gb_message( gb, _("pthread error!!!") );
			gb->ready = -1;
		}
		else
		{
			gb->conn[i].last_transfer = gettime();
		}

		gb->conn[i].start_time = gettime();
	}
	
	/* The real downloading will start now, so let's start counting	*/
	gb->start_time = gettime();
	gb->ready = 0;
  gb->start_byte = gb->bytes_done;
  // do some logging
#if ENABLE_GB_LOG
  char log_filepath[MAX_STRING];
	strcpy(log_file_name, "./");
	strcat(log_file_name, gb->conf->log_file);
  get_unique_filepath(log_filepath, log_file_name);
#ifndef NDEBUG
	printf("Log file : %s\n", log_filepath);
#endif
	if(!(gb->logfd = fopen(log_filepath, "w")))
	{
		printf("Error - can't open log.txt\n");
		exit(-1);
	}
  // write a header to logfd
#if LOG_OFFSET_WRITTEN 
  fprintf(gb->logfd, "Offset written\tTime(s)\tConnection\n");
#elif LOG_INORDER_OFFSET
  fprintf(gb->logfd, "Time(s)\tInorder offset\tInorder offset(MB)\n");
#endif
#endif
#if ENABLE_LOG_RECOVERY
  open_log_recovery(gb);
#endif
}

/* Close an gb connection						*/
void gb_close( gb_t *gb )
{
	int i;
	message_t *m;
	
	/* Terminate any thread still running				*/
	for( i = 0; i < gb->conf->num_connections; i ++ )
		/* don't try to kill non existing thread */
		if ( *gb->conn[i].setup_thread != 0 )
    {
			//pthread_kill( *gb->conn[i].setup_thread, SIGUSR1 );
      //FIXME there is no pthread_cancel on Android so we have to wait
			//pthread_cancel( *gb->conn[i].setup_thread );
      printf("Wait for setup_thread to finish\n");
      Pthread_join(*gb->conn[i].setup_thread, NULL);
    }   

	/* Delete any message not processed yet				*/
	while( gb->message )
	{
		m = gb->message;
		gb->message = gb->message->next;
		free( m );
	}
	
	/* Close all connections and local file				*/
	close( gb->outfd );
	for( i = 0; i < gb->conf->num_connections; i ++ )
		conn_disconnect( &gb->conn[i] );
#if ENABLE_GB_LOG
	fclose(gb->logfd);
#endif
#if ENABLE_LOG_RECOVERY
  close_log_recovery(gb);
#endif
  for (i = 0; i < gb->conf->num_connections; ++i)
    free(gb->conn[i].local_if);
  free( gb->conn );
	free( gb );
  close_cur_player_pos();
}

/* Check if there is any problem of getting the file over multiple connections or not
 * Construct then send an HTTP request, finally, get the response, extract its status 
 */
void *setup_thread( void *c )
{
	conn_t *conn = c;
	if (conn_setup( conn ))
	{
		conn->last_transfer = gettime();
		if (conn_exec(conn))
		{
			conn->last_transfer = gettime();
			conn->enabled = 1;
			conn->state = 0;
			return( NULL );
		}
	}
	conn_disconnect( conn );
	conn->state = 0;
	return( NULL );
}

/* Add a message to the gb->message structure				*/
void gb_message( gb_t *gb, char *format, ... )
{
	message_t *m = malloc( sizeof( message_t ) ), *n = gb->message;
	va_list params;
	
	memset( m, 0, sizeof( message_t ) );
	va_start( params, format );
	vsnprintf( m->text, MAX_STRING, format, params );
	va_end( params );
	
	if( gb->message == NULL )
	{
		gb->message = m;
	}
	else
	{
		while( n->next != NULL )
			n = n->next;
		n->next = m;
	}
}

/* send GET request for all chunks in subsegment */
/* return 0: no request sent
 * return 1: some request sent
 */
int send_http_request_for_subsegment(conn_t *conn, subsegment_t *subseg_p)
{
	if(subseg_p == NULL)
		return 0;
  if (conn->fd == -1 && !conn_init(conn)) {
    fprintf(stderr, "Reconnect fail!\n");
    exit(1); // TODO return to 1 link mode
  }

	chunk_t *chunk_p = subseg_p->chunk_list;
	while (chunk_p) {
		conn->http[0].request[0]='\0';
		conn->http[0].headers[0]='\0';
    double start_send_time = gettime();
		gb_get_request(conn, chunk_p->start_byte, chunk_p->last_byte);
    printf("GET request sending time: %.2lf\n", gettime() - start_send_time);
		conn->last_transfer = gettime();
		chunk_p = chunk_p->next;
	}
	conn->pipeline_requests_sent = 1;
	conn->state = 1;
	return 1;
}

void gb_get_request(conn_t *conn, long long int first_byte, long long int last_byte)
{
	char s[MAX_STRING];
	int i;

	conn->http->request[0] = '\0';
	snprintf(s, MAX_STRING, "%s%s", conn->dir, conn->file);
	conn->http->firstbyte = first_byte;
	conn->http->lastbyte = last_byte;
	http_get(conn->http, s);
	http_addheader(conn->http, "User-Agent: %s", conn->conf->user_agent);
	for(i = 0; i < conn->conf->add_header_count; i++)
		http_addheader(conn->http, "%s", conn->conf->add_header[i]);
	http_addheader(conn->http, "Connection: keep-alive");
	http_addheader(conn->http, "" );
	write( conn->http->fd, conn->http->request, strlen( conn->http->request ) );
}

/*
 * return 1 on successfully received http header response
 */
int gb_receive_http_header_response(http_t *conn)
{
	int i = 0;
	char s[2] = " ", *s2;
	int size = 0;

	*conn->headers = 0;

	while( 1 )
	{
		size = read(conn->fd, s, 1);
		if( size <= 0 )
		{
			sprintf( conn->headers, _("Connection gone.\n") );
			return( 0 );
		}
		if( *s == '\r' )
		{
			continue;
		}
		else if( *s == '\n' )
		{   
			if( i == 0 ) 
				break;
			i = 0;
		}   
		else
		{   
			i ++; 
		}   

		strncat( conn->headers, s, MAX_QUERY );
	}   
	return 1;
}

void gb_init_segment_setup( gb_t *gb )
{
	int i;
	chunk_t *chunk_temp = NULL;
	segment_t *seg_temp = NULL;

	memset(&gb->offset_info, 0, sizeof(offset_info_t));
	
  long long fixed_segment_size_bytes = gb->conf->fixed_segment_size_KB * 1024;
	if(gb->conf->num_of_segments == 0)
  {
		gb->offset_info.init_seg_size = (fixed_segment_size_bytes == 0) ?
                     gb->size : min(gb->size, fixed_segment_size_bytes);//gb->size / gb->conf->num_of_segments;
   // if (gb->conf->fixed_segment_mode)
    gb->conf->num_of_segments = (fixed_segment_size_bytes == 0) ? 1 : (gb->size / fixed_segment_size_bytes);
    if (gb->size % fixed_segment_size_bytes)
      gb->conf->num_of_segments++;
  }
	else
  {
		gb->offset_info.init_seg_size = gb->size / gb->conf->num_of_segments;
   // if (gb->conf->fixed_segment_mode)
    fixed_segment_size_bytes = gb->size / gb->conf->num_of_segments;
  }

	add_new_chunk(&chunk_temp, 0, gb->offset_info.init_seg_size);
	add_new_segment(&gb->offset_info, 0, chunk_temp, gb->offset_info.init_seg_size);
	add_new_subsegment(gb, 0, gb->conf->initial_chunk_rate[0], gb->conf->initial_chunk_rate[1], gb->conf->fixed_subsegment_mode);

	for(i = 0; i < gb->conf->num_connections ; i++)
	{
		chunk_temp = get_last_chunk(gb->offset_info.head->chunk_list);
		if(chunk_temp->last_byte == gb->size - 1)
			gb->conn[i].last_segment_flag = 1;

		if(gb->offset_info.head->subsegment[i] == NULL)
		{
			gb->conn[i].currentbyte = gb->size;
			gb->conn[i].lastbyte = gb->size + 1; // These are meaningless offsets.
			continue;
		}
		gb->offset_info.connection[i] = gb->offset_info.head;
		gb->conn[i].currentbyte = gb->offset_info.head->subsegment[i]->chunk_list->start_byte;
		gb->conn[i].lastbyte = gb->offset_info.head->subsegment[i]->chunk_list->last_byte;
		printf("Downloading %lld-%lld (%lld) using conn. %i\n",
        gb->conn[i].currentbyte, gb->conn[i].lastbyte, gb->offset_info.connection[i]->subsegment[i]->size, i);
	}

	gb->qos_bytes_per_second = (int)(gb->conf[0].qos_kbps * 1000 / 8);
	gb->qos_bytes = 0;

	//display_offset_info(&gb->offset_info);
}

void gb_update_goodput(gb_t *gb)
{
	double current_time = gettime();
  int index = 0;
  for (index = 0; index < gb->conf->num_connections; ++index) {
    // update bytes_per_second of both connections; bad code, just change to a for(index= 0, 1)
    if(gb->conn[index].bytes_done != 0)	{
      if(gb->offset_info.completed_segment_count == 0) {
        gb->conn[index].bytes_per_second = gb->conn[index].ewma_bytes_per_second =
          (long long int)((double)gb->conn[index].bytes_done / (current_time - gb->conn[index].start_time));
      } else {
        gb->conn[index].bytes_per_second =
          (long long int)((double)gb->conn[index].bytes_done / (current_time - gb->conn[index].start_time));
        gb->conn[index].ewma_bytes_per_second = 
          (long long int)(((double)(1 - gb->conf->ewma_factor) * gb->conn[index].ewma_bytes_per_second) + 
               (gb->conf->ewma_factor * gb->conn[index].bytes_per_second));
      }
    }
  }

#ifdef DEBUG
  printf("Instantaneous goodput of connections:");
  for (index = 0; index < gb->conf->num_connections; ++index)
    printf("    %d - %lld Bytes/sec", index, gb->conn[index].bytes_per_second);
  printf("\n");

  printf("EWMA goodput of connections: ");
  for (index = 0; index < gb->conf->num_connections; ++index)
    printf("    %d - %lld Bytes/sec", index, gb->conn[index].ewma_bytes_per_second);
  printf("\n");
#endif

  reset_bytes_done_start_time(gb);
	gb->offset_info.fast_conn = get_fast_conn_index(gb);
}

/*
 * Do segmentation and subsegmentation
 * return 0: no segmentation
 * return 1: no segmentation
 */
int gb_setup_next_segment(gb_t *gb, int index)
{
	conn_t *target = &gb->conn[index];
	conn_t *another = &gb->conn[!index];
  int another_index = !index;
  int link_mode = DUAL_LINK_MODE;
	int conn_order_case = get_conn_order_case(gb, index);/* decide this conn is ahead or behind the other */
	segment_t *next_seg = NULL;
	chunk_t *chunk_p = NULL;
  long long int next_segment_size = 0;
  int performed_recovery = 0;

	if(gb->conn[index].last_segment_flag == 1)
		return 0;

#ifdef DEBUG
	print_info_before_setup_next_segment(gb, index);
  printf("Connection order case: %d\n", conn_order_case);
#endif

	if(conn_order_case == BEHIND_SEG) {
		if(gb->offset_info.single_link_mode_flag == 1) {
			gb->offset_info.conn_idle[index] = 1;
			return 0;
		}
	} else if(conn_order_case == AHEAD_SEG) {
		gb_update_goodput(gb);
    performed_recovery = perform_recovery(gb,index);
    if (!performed_recovery) {
      chunk_p = get_last_chunk(gb->offset_info.tail->chunk_list);
      if(gb->conf->minimum_energy_mode && chunk_p->last_byte == gb->size - 1)
        return 0;
    }
	} else if (conn_order_case == ONE_SEG) {
		gb_update_goodput(gb);
    //if (gb->conn[index].bytes_per_second > gb->conn[another_index].bytes_per_second) // only fast conn can dis other conn
    if (gb->conn[index].currentbyte < gb->conn[another_index].currentbyte) // only fast conn can dis other conn
      performed_recovery = perform_recovery(gb,index);
	} else {
    assert(0); // No case like this
  }

	if(!performed_recovery) {
    gb_update_goodput(gb);
    if ((next_segment_size = gb_determine_next_segment_size(gb, index)) == 0)
      return 0;
    if(gb->conf->minimum_energy_mode && 
       gb->offset_info.completed_segment_count > 0 && gb->conf->num_connections != 1) {
        link_mode = choose_link_mode(gb, index, next_segment_size);
        print_link_mode(link_mode);
    }
    gb_determine_next_subsegment(gb, index, link_mode);
  }

#ifdef DEBUG
  print_info_after_setup_next_segment(gb, index);
#endif
	return 1;
}

long long int gb_determine_next_segment_policy(gb_t *gb, int index, int energy_mode)
{
	int goodput = 0;
	long long int segment_size = 0;

	if(energy_mode == 0)
		goodput = gb->conn[0].bytes_per_second + gb->conn[1].bytes_per_second;
	else if(energy_mode == 1)
		goodput = gb->conn[0].bytes_per_second;
	else if(energy_mode == 2)
		goodput = gb->conn[1].bytes_per_second;

	update_inorder_offset(gb);

        printf("***[energy_mode:%d] goodput: %d Mbps\n", energy_mode, goodput / 1000000);
	if(gb->qos_bytes)
		segment_size = (long long int)((double)(gb->inorder_offset - gb->qos_bytes + 1) / gb->qos_bytes_per_second * goodput * gb->conf->BW_factor_for_stage);

  /* if segment size is too small, such as 0, set it to initial segment size */
	if(segment_size < gb->offset_info.init_seg_size)
		segment_size = gb->offset_info.init_seg_size;

	return segment_size;
}

int gb_determine_next_segment(gb_t *gb, int index, int energy_mode)
{
	long long int segment_size = 0, remaining_chunks_size = 0;
	chunk_t *chunk_p = NULL, *temp = NULL;
	segment_t *segment_p = NULL;

	if(gb->conf->fixed_segment_mode)
	{
	//	if(energy_mode == 0)
			segment_size = gb->offset_info.init_seg_size;
	//	else
	//		segment_size = gb->size;
	}
	else
		segment_size = gb_determine_next_segment_policy(gb, index, energy_mode);

	if(gb->size - 1 - (gb->offset_info.last_seg_offset + segment_size) < gb->offset_info.init_seg_size)
		segment_size = gb->size - gb->offset_info.last_seg_offset - 1;
	if(segment_size == 0)
		return 0;

	if(gb->offset_info.remaining_chunks != NULL)
	{
		chunk_p = gb->offset_info.remaining_chunks;
		while(chunk_p != NULL)
		{   
			remaining_chunks_size += chunk_p->size;
			chunk_p = chunk_p->next;
		}
		chunk_p = gb->offset_info.remaining_chunks;
		gb->offset_info.remaining_chunks = NULL;
	}

	add_new_chunk(&chunk_p, get_start_byte_next_segment(&gb->offset_info), segment_size);
	add_new_segment(&gb->offset_info, 0, chunk_p, segment_size + remaining_chunks_size);
	return 1;
}
long long int gb_determine_next_segment_size(gb_t *gb, int index)
{
  long long int new_seg_size = 0, remaining_chunks_size = 0;
  chunk_t *new_chunk_list = NULL, *tmp_chunk = NULL;
  segment_t *new_segment = NULL;

  //if (gb->size - 1 - gb->offset_info.last_seg_offset < gb->offset_info.init_seg_size)
  //if remaining size < 0.4 * fixed_seg_size
  //  new_seg_size = gb->size - gb->offset_info.last_seg_offset - 1; // exclude the last, small portion from the last segment
  //else if remaining size >= 0.4 * fixed_seg_size
  //  new_seg_size = gb->size - gb->offset_info.last_seg_offset - 1; // include the last, small portion to the last segment
  if (gb->conf->fixed_segment_mode) {
    new_seg_size = gb->offset_info.init_seg_size;
    if (is_using_only_one_link(gb))
      new_seg_size /= 2;
  } else {
    new_seg_size = gb_determine_next_segment_policy(gb, 0, 0); //FIXME this is temporary by now
  }

  if (gb->size - 1 - (gb->offset_info.last_seg_offset + new_seg_size) < gb->offset_info.init_seg_size)
    new_seg_size = gb->size - gb->offset_info.last_seg_offset - 1; // include the last, small portion to the last segment


  if (gb->offset_info.remaining_chunks != NULL) {
    //consolidate remaining chunks
    new_chunk_list = gb->offset_info.remaining_chunks;
    while (new_chunk_list != NULL) {
      remaining_chunks_size += new_chunk_list->size;
      new_chunk_list = new_chunk_list->next;
    }
    new_chunk_list = gb->offset_info.remaining_chunks;
    gb->offset_info.remaining_chunks = NULL;
    // add additional segsize if necessary
    if (new_seg_size != 0) {
      add_new_chunk(&new_chunk_list, get_start_byte_next_segment(&gb->offset_info), new_seg_size);
      new_seg_size += remaining_chunks_size;
    } else {
      new_seg_size = remaining_chunks_size;
    }
    add_new_segment(&gb->offset_info, 0, new_chunk_list, new_seg_size);
  } else {
    if (new_seg_size != 0) {
      add_new_chunk(&new_chunk_list, get_start_byte_next_segment(&gb->offset_info), new_seg_size);
      add_new_segment(&gb->offset_info, 0, new_chunk_list, new_seg_size);
    }
  }
  return new_seg_size;
}

void gb_determine_next_subsegment(gb_t* gb, int index, int link_mode)
{
	if(link_mode) //energy - 1 is the index of LTE (0) or WIFI(1)
		add_new_subsegment(gb, link_mode - 1, 1, 0, gb->conf->fixed_subsegment_mode);
	else if(gb->conf->fixed_subsegment_mode)
		add_new_subsegment(gb, index, 
                       gb->conf->initial_chunk_rate[index], gb->conf->initial_chunk_rate[!index],
                       gb->conf->fixed_subsegment_mode);
	else
		add_new_subsegment(gb, index, 
                       gb->conn[index].bytes_per_second, gb->conn[!index].bytes_per_second, 
                       gb->conf->fixed_subsegment_mode);
}

double get_segment_download_time_link(gb_t *gb, long long int seg_size, int link_index)
{
  return (double)seg_size / (double)gb->conn[link_index].bytes_per_second;
}

void gb_get_ip(char **addr_list)
{
	struct ifreq *ifr;		// ethernet data struct
	struct sockaddr_in *sin;

	struct ifconf ifcfg;	// ethernet configuration struct
	int fd;
	int n;
	int numreqs = 30;
	fd = socket(AF_INET, SOCK_DGRAM, 0);

	int nic_count=0;

	memset(&ifcfg, 0, sizeof(ifcfg));
	ifcfg.ifc_buf = NULL;
	ifcfg.ifc_len = sizeof(struct ifreq) * numreqs;
	ifcfg.ifc_buf = malloc(ifcfg.ifc_len);

	for(;;)
	{
		ifcfg.ifc_len = sizeof(struct ifreq) * numreqs;
		ifcfg.ifc_buf = realloc(ifcfg.ifc_buf, ifcfg.ifc_len);
		if (ioctl(fd, SIOCGIFCONF, (char *)&ifcfg) < 0)
		{
			perror("SIOCGIFCONF ");
			exit(1);
		}
		break;
	}

	ifr = ifcfg.ifc_req;
	for (n = 0; n < ifcfg.ifc_len; n+= sizeof(struct ifreq))
	{
		sin = (struct sockaddr_in *)&ifr->ifr_addr;

		if ( ntohl(sin->sin_addr.s_addr) != INADDR_LOOPBACK)
		{
			if(nic_count < 2)
			{
				strcpy(addr_list[nic_count], inet_ntoa(sin->sin_addr));
				nic_count++;
			}
		}
		ifr++;
	}
	close(fd);
}

int update_inorder_offset(gb_t *gb)
{
  int i = 0;
  long long int conn_byte[2];
  long long int found_inorder_offset = 0;

  for (i = 0; i < gb->conf->num_connections; ++i) {
    conn_byte[i] = gb->conn[i].currentbyte;
    gb->offset_info.conn_idle[i] = (gb->conn[i].currentbyte == gb->conn[i].lastbyte + 1) ? 1 : 0;
  }

  if (!is_using_only_one_link(gb)) {
    if (gb->offset_info.conn_idle[0] && gb->offset_info.conn_idle[1]) { // both links are idle, happens at the end of file downloading
      found_inorder_offset = gb->size - 1;
    } else { // are using both links
      //printf("update_inorder_offset: Is using 2 links\n");
      if (gb->bytes_done == gb->size)
        found_inorder_offset = gb->size - 1;

      /* search for the chunk with smallest start byte offset of each link */
      segment_t *segment_p = gb->offset_info.head;
      while (segment_p) {
        for (i = 0; i < gb->conf->num_connections; ++i) {
          if (segment_p != gb->offset_info.connection[i])
            if (segment_p->subsegment[i])
              if (segment_p->subsegment[i]->chunk_list->start_byte < conn_byte[i])
                conn_byte[i] = segment_p->subsegment[i]->chunk_list->start_byte;
        }
        segment_p = segment_p->next;
      }
      found_inorder_offset = (conn_byte[0] < conn_byte[1]) ? conn_byte[0] : conn_byte[1];
    }
  } else {
    //printf("update_inorder_offset: Is using 1 link\n");
    for (i = 0; i < gb->conf->num_connections; ++i) {
      if (gb->offset_info.conn_idle[i] == 0) {
        found_inorder_offset = conn_byte[i];
        assert(gb->offset_info.conn_idle[!i] == 1);
        break;
      }
    }
  }
  
  /* check the correctness */
  if (!(gb->inorder_offset <= found_inorder_offset)) {
    fprintf(stderr, "FATAL ERROR: current inorder offset: %lld    found inorder offset: %lld\n",
                    gb->inorder_offset, found_inorder_offset);
    assert(gb->inorder_offset <= found_inorder_offset);
  }

  gb->inorder_offset = found_inorder_offset;
  return -1;
}

int update_inorder_offset_old(gb_t *gb)
{
	segment_t *segment_p = gb->offset_info.head;
	long long int conn_byte[2];
	int i = 0;

	conn_byte[0] = gb->conn[0].currentbyte;
	conn_byte[1] = gb->conn[1].currentbyte;

	if (gb->bytes_done == gb->size) {
		gb->inorder_offset = gb->size-1;
		if (gb->conn[0].lastbyte == gb->size-1)
			return 0;
		else if (gb->conn[1].lastbyte == gb->size-1)
			return 1;
	}
  // search for last inorder offset
	while (segment_p) {
		for(i = 0 ; i < 2; ++i) {
			if (segment_p != gb->offset_info.connection[i])
        if (segment_p->subsegment[i])
          if (conn_byte[i] > segment_p->subsegment[i]->chunk_list->start_byte)
            conn_byte[i] = segment_p->subsegment[i]->chunk_list->start_byte;
		}
		segment_p = segment_p->next;
	}
	if(conn_byte[0] < conn_byte[1])	{
		gb->inorder_offset = conn_byte[0];
		return 0;
	}	else {
		gb->inorder_offset = conn_byte[1];
		return 1;
	}
}

void mark_qos_start_time(gb_t *gb)
{
	double current_time = gettime();

	//if(current_time - gb->start_time >= 3 && gb->qos_start_flag == 0)
	if(gb->qos_start_flag == 0 && (get_cur_player_pos() > 0.0))
	{
		gb->qos_start_time = current_time;
		gb->qos_start_flag = 1;
	}
}

int exception_for_read(gb_t *gb, long long int size, int index)
{
	if( size == -1 )
	{
		if( gb->conf->verbose )
		{
			gb_message( gb, _("Error on connection %i! ""Connection closed"), index);
          //exit(1);
		}
		gb->conn[index].enabled = 0;
		conn_disconnect( &gb->conn[index]);
		return 1;
	}
	else if( size == 0 )
	{
		if( gb->conf->verbose )
		{
			if( gb->conn[index].currentbyte < gb->conn[index].lastbyte && gb->size != INT_MAX )
			{
				gb_message( gb, _("Connection %i unexpectedly closed"), index );
			}
			else
			{
				gb_message( gb, _("Connection %i finished"), index );
			}
		}
		if( !gb->conn[0].supported )
		{
			gb->ready = 1;
		}
		gb->conn[index].enabled = 0;
		conn_disconnect( &gb->conn[index] );
		return 1;
	}
	return 0;
}

void checking_last_subseg(gb_t *gb, int index)
{
	if(get_last_chunk(gb->offset_info.connection[index]->subsegment[index]->chunk_list)->last_byte == gb->size - 1)
		gb->conn[index].last_segment_flag = 1;
}

void write_received_data(gb_t *gb, int conn_index, long long int size)
{
	lseek( gb->outfd, gb->conn[conn_index].currentbyte, SEEK_SET );
	if( write( gb->outfd, buffer, size ) != size )
	{
		gb_message( gb, _("Write error!") );
		gb->ready = -1;
		return;
	}
	gb->conn[conn_index].currentbyte += size;
	gb->bytes_done += size;
	gb->conn[conn_index].bytes_done += size;
  double current_time = gettime() - gb->start_time;
#if ENABLE_GB_LOG
#if LOG_OFFSET_WRITTEN 
  fprintf(gb->logfd, "%lld\t%lf\t%d\n", gb->conn[conn_index].currentbyte + size / 2, current_time, conn_index);
#elif LOG_INORDER_OFFSET
  fprintf(gb->logfd, "%.2lf\t%lld\t%.3lf\n", current_time, gb->inorder_offset, (double) gb->inorder_offset / (1024 * 1024));
#endif
#endif
}

void return_downloading_for_idle_conn(gb_t *gb, int conn_index)
{
	if(gb->offset_info.connection[conn_index] == NULL) // idle conn?
	{
		subsegment_t *subseg_p = gb->offset_info.tail->subsegment[conn_index];

		gb->offset_info.connection[conn_index] = gb->offset_info.tail;
		gb->conn[conn_index].currentbyte = subseg_p->chunk_list->start_byte;
		gb->conn[conn_index].lastbyte = subseg_p->chunk_list->last_byte;
		checking_last_subseg(gb, conn_index);

		if(gb_receive_http_header_response(gb->conn[conn_index].http))
		{        
			gb->conn[conn_index].enabled = 1;  
			gb->conn[conn_index].state = 0;  
			gb->conn[conn_index].pipeline_requests_sent = 0;  
			gb->offset_info.conn_idle[conn_index] = 0;
		}
	}
}

void finish_downloading_subseg(gb_t *gb, int index)
{
	if (gb->conn[index].pipeline_requests_sent) { // receive next subsegment
		if (gb_receive_http_header_response(gb->conn[index].http)) {
			gb->conn[index].enabled = 1;
			gb->conn[index].state = 0;
		}

		printf("Conn %d : %d -> ", index, gb->offset_info.connection[index]->id);
		gb->offset_info.connection[index] = get_first_next_segment_assigned_to_conn(gb->offset_info.connection[index], index);
		printf("%d\n", gb->offset_info.connection[index]->id);

		gb->conn[index].currentbyte = gb->offset_info.connection[index]->subsegment[index]->chunk_list->start_byte;
		gb->conn[index].lastbyte = gb->offset_info.connection[index]->subsegment[index]->chunk_list->last_byte;

		checking_last_subseg(gb, index);

		if (get_first_next_segment_assigned_to_conn(gb->offset_info.connection[index], index) == NULL)
			gb->conn[index].pipeline_requests_sent = 0;
	} else if (gb->conn[index].lastbyte == gb->size - 1 || gb->conn[!index].lastbyte == gb->size - 1 ||
             (gb->offset_info.single_link_mode_flag == 1 && index == INDEX_LTE)) {
		gb->offset_info.connection[index] = NULL;
		conn_disconnect(&gb->conn[index]);
		printf("Close connection %d\n", index);
	} else {
		gb->offset_info.connection[index] = NULL;
  }

	while (delete_finished_head_segment(&gb->offset_info));
}


void check_conn(gb_t *gb)
{
	/* Look for aborted connections and attempt to restart them.	*/
	int i = 0;

	for( i = 0; i < gb->conf->num_connections; i ++ )
	{
		if( !gb->conn[i].enabled && gb->conn[i].currentbyte < gb->conn[i].lastbyte )
		{
			if( gb->conn[i].state == 0 )
			{	
				// Wait for termination of this thread
				pthread_join(*(gb->conn[i].setup_thread), NULL);

				conn_set( &gb->conn[i], gb->url->text );
				gb->url = gb->url->next;

				if( gb->conf->verbose >= 2 )
					gb_message( gb, _("check_conn: Connection %i downloading from %s:%i using interface %s"),
							i, gb->conn[i].host, gb->conn[i].port, gb->conn[i].local_if );

				gb->conn[i].state = 1;
				if( pthread_create( gb->conn[i].setup_thread, NULL, setup_thread, &gb->conn[i] ) == 0 )
				{
					gb->conn[i].last_transfer = gettime();
				}
				else
				{
					gb_message( gb, _("pthread error!!!") );
					gb->ready = -1;
				}
			}
			else
			{
				if( gettime() > gb->conn[i].last_transfer + gb->conf->reconnect_delay )
				{
					//FIXME there is no pthread_cancel on Android so we have to wait
					//pthread_cancel( *gb->conn[i].setup_thread );

					printf("algo_general.c - Wait for setup_thread to finish\n");
					Pthread_join(*gb->conn[i].setup_thread, NULL);
					gb->conn[i].state = 0;
				}
			}
		}
	}
}
void checking_last_segment(gb_t *gb)
{
	int i;
	for(i = 0; i < gb->conf->num_connections; i++)
	{
		segment_t *next_seg = NULL;
		chunk_t *chunk_p = NULL;

		if(gb->offset_info.connection[i] != NULL)
			next_seg = get_first_next_segment_assigned_to_conn(gb->offset_info.connection[i], i);
		if(next_seg != NULL)
		{
			chunk_p = get_last_chunk(next_seg->chunk_list);
			if(chunk_p->last_byte == gb->size - 1)
				gb->conn[i].last_segment_flag = 1;
		}
	}
}
void open_log_recovery(gb_t *gb)
{
  char log_recovery_filepath[MAX_STRING] = "";
  get_unique_filepath(log_recovery_filepath, LOG_RECOVERY_FILEPATH);
  if((gb->log_recovery = fopen(log_recovery_filepath, "w")) == NULL) {
    printf("WARNING: Cannot create log recovery file\n");
    return;
  }
  fprintf(gb->log_recovery, "Recovery Time\n");
}
void close_log_recovery(gb_t *gb)
{
  if (gb->log_recovery)
    fclose(gb->log_recovery);
}
void print_info_before_setup_next_segment(gb_t *gb, int conn_index)
{
  int index = 0;
	printf("Before sending a next request\n");
  printf("Last downloaded byte: %lld\n", gb->last_downloaded_byte);
  for (index = 0; index < gb->conf->num_connections; ++index) {
	  printf("conn %d - currentbyte : %lld lastbyte : %lld\n", index, gb->conn[index].currentbyte, gb->conn[index].lastbyte);
  }
  printf("Currently finishing connection: %d\n", conn_index);
	fprintf(stderr, "----------BEFORE----------\n");
	display_offset_info(gb, conn_index);
	printf("--------------------------\n");
}	
void print_info_after_setup_next_segment(gb_t *gb, int conn_index)
{
	printf("\n----------AFTER----------\n");
	display_offset_info(gb, conn_index);
	printf("-------------------------\n");
}
void print_chunk_info(chunk_t *chunk_p)
{
  long long int start_byte = 0, last_byte = 0, size = 0;
  start_byte = chunk_p->start_byte;
  last_byte = chunk_p->last_byte;
  size = chunk_p->size;
  printf("- Start: %lld,%.6lld    Last: %lld,%.6lld    Size: %lld,%.6lld\n", 
      start_byte / ONE_MIL, start_byte % ONE_MIL, last_byte / ONE_MIL, last_byte % ONE_MIL, size / ONE_MIL, size % ONE_MIL);
}
void print_conn_pos(gb_t *gb, int cur_fin_conn_index, int conn_index)
{
  long long int currentbyte = gb->conn[conn_index].currentbyte, lastbyte = gb->conn[conn_index].lastbyte;
  if (conn_index == cur_fin_conn_index) printf("*");
  printf("Connection %d - Segment%s: %d    Current byte: %lld,%.6lld    Last byte: %lld,%.6lld\n", 
         conn_index,
         (gb->offset_info.connection[conn_index]->recovery_segment == 1) ? "#" : "",
         gb->offset_info.connection[conn_index]->id, 
         currentbyte / ONE_MIL, currentbyte % ONE_MIL,
         lastbyte / ONE_MIL, lastbyte % ONE_MIL);
}
void display_offset_info(gb_t *gb, int cur_fin_conn_index)
{
  offset_info_t *offset_info = &gb->offset_info;
  int index = 0;
	segment_t *segment_p = offset_info->head;
	chunk_t *chunk_p = NULL;
	
	while (segment_p) {
    long long int seg_size = segment_p->size;
		printf("Segment%s %d (size: %lld,%.6lld)\n", 
        (segment_p->recovery_segment == 1) ? "#" : "",
        segment_p->id, seg_size / ONE_MIL, seg_size % ONE_MIL);
    // print all chunks of the segment
		chunk_p = segment_p->chunk_list;
		while (chunk_p) {
      print_chunk_info(chunk_p);
			chunk_p = chunk_p->next;
		}
    // print all subsegment of the segment
    for (index = 0; index < gb->conf->num_connections; ++index) {
			if(segment_p->subsegment[index]) {
				printf("\tSubseg %d\n", index);
				chunk_p = segment_p->subsegment[index]->chunk_list;
				while (chunk_p) {
          printf("\t");
          print_chunk_info(chunk_p);
					chunk_p = chunk_p->next;
				}
			}
    }
		segment_p = segment_p->next;
	}

  for (index = 0; index < gb->conf->num_connections; ++index) {
    if(offset_info->connection[index])
      print_conn_pos(gb, cur_fin_conn_index, index);
  }
}

void reset_bytes_done_start_time(gb_t *gb)
{
  int index = 0;
  for (index = 0; index < gb->conf->num_connections; ++index) {
    gb->conn[index].bytes_done = 0; //reset bytes_done of both connections
    gb->conn[index].start_time = gettime(); //reset start_time of both conns to now
  }
}
void print_link_mode(int link_mode)
{
  switch (link_mode)
  {
    case LTE_SINGLE_LINK_MODE:
    	printf("%c[%d;%d;%dmLTE Single Link Mode%c[%d;%dm\n", 
            0x1B, BRIGHT, RED, BG_BLACK,0x1B, WHITE, ATTR_OFF);
      break;
    case WIFI_SINGLE_LINK_MODE:
    	printf("%c[%d;%d;%dmWIFI Single Link Mode%c[%d;%dm\n", 
            0x1B, BRIGHT, GREEN, BG_BLACK,0x1B, WHITE, ATTR_OFF);
      break;
    case DUAL_LINK_MODE:
    	printf("%c[%d;%d;%dmDual Link Mode%c[%d;%dm\n", 
            0x1B, BRIGHT, CYAN, BG_BLACK,0x1B, WHITE, ATTR_OFF);
      break;
    default:
    	printf("%c[%d;%d;%dmWARNING: Unrecognized recovery decision!%c[%d;%dm\n", 
            0x1B, BRIGHT, RED, BG_BLACK,0x1B, WHITE, ATTR_OFF);
  }
}

long long int get_remaining_file_size(gb_t *gb)
{
  return gb->size - get_start_byte_next_segment(&gb->offset_info);
}

int is_using_only_one_link(gb_t *gb)
{
  //assert (!(gb->offset_info.conn_idle[0] && gb->offset_info.conn_idle[1])); // happens at the end of file downloading
  return (gb->offset_info.conn_idle[0] != gb->offset_info.conn_idle[1]);
}
/*
long long int get_bottle_neck_conn_current_byte(gb_t *gb)
{
  return gb->conn[get_bottle_neck_conn_index(gb)].currentbyte;
}
int get_bottle_neck_conn_goodput(gb_t *gb)
{
  return gb->conn[get_bottle_neck_conn_index(gb)].bytes_per_second;
}
*/
