// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#include "recovery.h"
#include "algo_general.h"
#include "util.h"
#include "gb.h"
#define NDEBUG
#define SEND_PIPELINE_REQ_THRS_FACTOR 10
void algo_general( gb_t *gb )
{
  fd_set fds[1];
  int hifd, i;
  long long int remaining; // remaining is remaining num of bytes of a connection
  long long int num_bytes_received; // num_bytes_received is num of received bytes from a connection
  struct timeval receive_timeout[1];
  double current_time = gettime();

  mark_qos_start_time(gb);// set gb->qos_start_time and qos_start_flag

  /* Wait for data on (one of) the connections      */
  FD_ZERO(fds);
  hifd = 0;
  for( i = 0; i < gb->conf->num_connections; i ++ ) {
    if( gb->conn[i].enabled && gb->conn[i].fd != -1)
      FD_SET( gb->conn[i].fd, fds );
    hifd = max( hifd, gb->conn[i].fd );
  }
  if( hifd == 0 ) {
    /* No connections yet. Wait...        */
    usleep( 100000 );
    goto conn_check;
  } else {
    receive_timeout->tv_sec = 0;
    receive_timeout->tv_usec = 100000;
    /* A select() error probably means it was interrupted
       by a signal, or that something else's very wrong...  */
    if( select( hifd + 1, fds, NULL, NULL, receive_timeout ) == -1 ) {
      gb->ready = -1;
      return;
    }
  }

  /* Handle connections which need attention      */
  for (i = 0; i < gb->conf->num_connections; ++i) {
    if (gb->conn[i].enabled && (gb->conn[i].fd != -1) && FD_ISSET(gb->conn[i].fd, fds)) {
      gb->conn[i].last_transfer = current_time = gettime();
      remaining = gb->conn[i].lastbyte - gb->conn[i].currentbyte + 1; 
    
      if(remaining >= gb->conf->buffer_size)
        num_bytes_received = read( gb->conn[i].fd, buffer, gb->conf->buffer_size);
      else
        num_bytes_received = read( gb->conn[i].fd, buffer, remaining);

      if (exception_for_read(gb, num_bytes_received, i))
        continue;

      write_received_data(gb, i, num_bytes_received);

      /* Send request for next segment in advance - pipelining */
      if ((gb->offset_info.connection[i]->subsegment[i]->size / SEND_PIPELINE_REQ_THRS_FACTOR > remaining
          || remaining <= num_bytes_received) &&
          !gb->conn[i].pipeline_requests_sent && !gb->offset_info.conn_idle[i]) {
        if (gb_setup_next_segment(gb, i)) { // segmentation is performed
          send_http_request_for_subsegment(&gb->conn[i], gb->offset_info.tail->subsegment[i]);//set pipeline_sent
          if (gb->offset_info.tail->recovery_segment == 0) {
            if (send_http_request_for_subsegment(&gb->conn[!i], gb->offset_info.tail->subsegment[!i]) != 0)//set pipeline
              return_downloading_for_idle_conn(gb, !i); // if other conn is idle, pipeline_requests_sent = 0 TODO reconnect a conn if it is disconnected, fd == -1
          } else {
            return_downloading_for_recovered_conn(gb, !i); // create new conn, pipeline_requests_sent = 0
          }
          checking_last_segment(gb);
        }
      }

      if (remaining <= num_bytes_received) {
        gb->conn[i].enabled = 0;
        if (finish_last_chunk_of_subseg(gb, i)) { // downloaded the last chunk in the subsegment, complete a subsegment
          segment_t *connection = gb->offset_info.connection[i];
          delete_subsegment(&connection->subsegment[i]);
          printf("Subsegment of conn %d of segment %d is completed.\n", i, connection->id); 
          finish_downloading_subseg(gb, i); // pipeline_requests_sent = 0 TODO disconnect LTE if in WIFI single link mode
          update_inorder_offset(gb); // update gb->inorder_offset
        }
      } else {
        update_inorder_offset(gb);
      }
    }
  }
  if( gb->ready )
    return;
  
conn_check:
  check_conn(gb);
  gb->bytes_per_second = (int) ( (double) ( gb->bytes_done - gb->start_byte ) / ( gettime() - gb->start_time ) );
  gb->finish_time = (int) ( gb->start_time + (double) ( gb->size - gb->start_byte ) / gb->bytes_per_second );
  if( gb->bytes_done == gb->size )
    gb->ready = 1;
}
