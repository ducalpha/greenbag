// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#include "recovery.h"
#include "player_time.h"
#include "gbsession.h"
#include "util.h"
#include "offset.h"
#include "energy.h"
#include <stdlib.h>
void print_player_state(int player_is_playing);
void print_recovery_start_msg(void);
void print_recovery_mode(int recovery_mode);


/* return 0: not recover
 * return 1: recovered
 */
int perform_recovery(gb_t *gb, int cur_fin_conn_index)
{
  if (gb->conf->recovery_mode == 0)
    return 0;
  int performed_recovery = 0;
	int another_index = !cur_fin_conn_index;
  int recovery_mode = decide_recovery_mode(gb, cur_fin_conn_index);

  if (recovery_mode == UNDECIDED_RECOVERY_MODE || recovery_mode == NO_RECOVERY_MODE) {
    printf("No recovery\n");
    performed_recovery = 0;
  } else {
    print_recovery_start_msg();
    print_recovery_mode(recovery_mode);

    gb->conn[another_index].enabled = 0;
    printf("Disconnect connection %d\n", another_index);
    conn_disconnect(&gb->conn[another_index]);

    switch (recovery_mode) {
      case RECOVERY_FOR_INSUFFICIENT_PLAYABLE_TIME:
        recover_outorder_portion_and_next_segment(gb, cur_fin_conn_index);//determine next seg = outorder portion + next segment
        break;
      default:
        printf("WARNING: Unrecognized recovery decision!\n");
    }

    gb_determine_next_subsegment(gb, cur_fin_conn_index, DUAL_LINK_MODE);
    performed_recovery = 1; 
    ++(gb->num_recoveries);
#if LOG_RECOVERY
    fprintf(gb->log_recovery, "%lf\n", gettime() - gb->start_time);
#endif
  }
  return performed_recovery;
}

int decide_recovery_mode(gb_t* gb, int conn_index)
{
	int another_index = !conn_index;
  int recovery_decision = NO_RECOVERY_MODE;
  long long int bottle_neck_size = 0;
  static long long int very_small_portion_threshold = 500; //500 bytes
  static int player_state = UNKNOWN_PLAYER_STATE;
  static int previous_player_state = UNKNOWN_PLAYER_STATE;
  static long long int start_pausing_byte = -1;
  static int recovered_once_for_insufficient_playabled_time = 0;

	offset_info_t *offset_info = &gb->offset_info;
	segment_t *segment_p = offset_info->connection[another_index];
  player_state = get_player_state();

	if (!(offset_info->single_link_mode_flag == 1 || segment_p == NULL ||
       segment_p->subsegment[another_index] == NULL)) { //|| segment_p->recovery_segment == 1)) {
    if (conn_index != get_fast_conn_index(gb)) {
      fprintf(stderr, "not fast connection, no recovery\n");
      recovery_decision = NO_RECOVERY_MODE;
    } else {
      if (!(offset_info->connection[conn_index]->recovery_segment == 1 && 
            offset_info->connection[another_index]->recovery_segment == 1)) { // both lnk in recovery seg
        get_bn_chunk_info(gb, conn_index, NULL, &bottle_neck_size, NULL);
        if ((get_cur_player_pos() > 0.0) &&
            (bottle_neck_size > very_small_portion_threshold) &&
            does_qos_miss_until_bn_chunk(gb, conn_index)) {
          recovery_decision = RECOVERY_FOR_INSUFFICIENT_PLAYABLE_TIME;
        } else {
         recovery_decision = NO_RECOVERY_MODE;
        }
      }
    }
  }
	return recovery_decision;
}

long long int get_total_goodput(void)
{
  int i = 0;
  long long int total_goodput = 0;
  for (i = 0; i < gb->conf->num_connections; ++i)
    total_goodput += gb->conn[i].bytes_per_second;
  return total_goodput;
}

void return_downloading_for_recovered_conn(gb_t *gb, int index)
{
	if(gb->offset_info.tail->subsegment[index]) {
    // TODO always do this?
		conn_t *conn = &gb->conn[index];
		if (!conn_init(conn)) {
			fprintf(stderr, "Reconnect fail!\n");
			exit(1); // TODO return to 1 link mode
		}

		subsegment_t *subseg_p = gb->offset_info.tail->subsegment[index];

		gb->offset_info.connection[index] = gb->offset_info.tail;
		gb->conn[index].currentbyte = subseg_p->chunk_list->start_byte;
		gb->conn[index].lastbyte = subseg_p->chunk_list->last_byte;
    checking_last_subseg(gb, index);

		send_http_request_for_subsegment(conn, subseg_p);

		if(gb_receive_http_header_response(conn->http)) {
			conn->enabled = 1;
			conn->state = 0;
			conn->pipeline_requests_sent = 0;
		}
	} else { // no subsegment --> bug? no reconnection?
		gb->offset_info.connection[index] = NULL;
		gb->conn[index].currentbyte = 1;
		gb->conn[index].lastbyte = 0;
	}
}
/*
 * receive a chunk in an uncontiguous subsegment
 * complete a chunk in a multiple-chunk subsegment
 * return 0: there is other chunk(s) --> continue downloading next chunk of the subseg
 * return 1: there is no other chunk --> finish last chunk of the subsegment
 */
int finish_last_chunk_of_subseg(gb_t *gb, int index)
{
	segment_t *connection = gb->offset_info.connection[index];
	delete_head_chunk(&connection->subsegment[index]->chunk_list);

	if (connection->subsegment[index]->chunk_list) { // there is other chunks in the subseg
		gb->conn[index].currentbyte = connection->subsegment[index]->chunk_list->start_byte;
		gb->conn[index].lastbyte = connection->subsegment[index]->chunk_list->last_byte;
		if(gb_receive_http_header_response(gb->conn[index].http)) {
			gb->conn[index].enabled = 1;
			gb->conn[index].state = 0;
		}
		return 0;
	}
	return 1;
}

void recover_outorder_portion(gb_t *gb, int conn_index)
{
	int another_index = !conn_index;
	offset_info_t *offset_info = &gb->offset_info;
	segment_t *segment_p = offset_info->connection[another_index];
	subsegment_t *subseg_p = segment_p->subsegment[another_index];
	chunk_t *chunk_list = subseg_p->chunk_list, *chunk_p = NULL;
	long long int segment_size = 0, additive_segment_size = 0;
	
	subseg_p->chunk_list = NULL;
	chunk_list->start_byte = gb->conn[another_index].currentbyte;
	chunk_list->size = chunk_list->last_byte - chunk_list->start_byte + 1;
	delete_subsegment(&segment_p->subsegment[another_index]);
	segment_p = get_first_next_segment_assigned_to_conn(segment_p, another_index);
	delete_finished_head_segment(offset_info);

	while(segment_p != NULL)
	{
		subseg_p = segment_p->subsegment[another_index];
		get_last_chunk(chunk_list)->next = subseg_p->chunk_list;
		subseg_p->chunk_list = NULL;
		delete_subsegment(&segment_p->subsegment[another_index]);
		segment_p = get_first_next_segment_assigned_to_conn(segment_p, another_index);
		delete_finished_head_segment(offset_info);
	}
	chunk_list = sorting_chunks(chunk_list);
	chunk_p = chunk_list;
	offset_info->remaining_chunks = chunk_list->next;
	chunk_p->next = NULL;

	offset_info->connection[another_index] = add_new_segment(offset_info, 1, chunk_p, chunk_p->size);
	gb_determine_next_subsegment(gb, !another_index, 0);
}

void recover_outorder_portion_and_next_segment(gb_t *gb, int conn_index)
{
	int another_index = !conn_index;
	long long int segment_size = 0, additive_segment_size = 0;

	offset_info_t *offset_info = &gb->offset_info;
	segment_t *segment_p = offset_info->connection[another_index];
	subsegment_t *subseg_p = segment_p->subsegment[another_index];
	chunk_t *chunk_list = subseg_p->chunk_list, *chunk_p = NULL;
	
	subseg_p->chunk_list = NULL;
	chunk_list->start_byte = gb->conn[another_index].currentbyte;
	chunk_list->size = chunk_list->last_byte - chunk_list->start_byte + 1;
	delete_subsegment(&segment_p->subsegment[another_index]);
	segment_p = get_first_next_segment_assigned_to_conn(segment_p, another_index);
	delete_finished_head_segment(offset_info);

	while (segment_p) {
		subseg_p = segment_p->subsegment[another_index];
		get_last_chunk(chunk_list)->next = subseg_p->chunk_list;
		subseg_p->chunk_list = NULL;
		delete_subsegment(&segment_p->subsegment[another_index]);
		segment_p = get_first_next_segment_assigned_to_conn(segment_p, another_index);
		delete_finished_head_segment(offset_info);
	}

	if(gb->conf->fixed_segment_mode)
		additive_segment_size = gb->offset_info.init_seg_size;
	else 
		additive_segment_size = gb_determine_next_segment_policy(gb, !another_index, 0);

  // TODO exclude the remaining file if it is too big
	if(gb->size - 1 - (gb->offset_info.last_seg_offset + additive_segment_size) < gb->offset_info.init_seg_size)
		additive_segment_size = gb->size - gb->offset_info.last_seg_offset - 1;
	
	if(additive_segment_size != 0)
		add_new_chunk(&chunk_list, get_start_byte_next_segment(offset_info), additive_segment_size);

	chunk_p = chunk_list;
	while (chunk_p) {
		segment_size += chunk_p->size;
		chunk_p = chunk_p->next;
	}

	offset_info->connection[another_index] = add_new_segment(offset_info, 1, chunk_list, segment_size);
}

void recover_one_time(gb_t *gb, int conn_index)
{
	int another_index = !conn_index;
	offset_info_t *offset_info = &gb->offset_info;
	segment_t *segment_p = offset_info->connection[another_index];
	subsegment_t *subseg_p = segment_p->subsegment[another_index];
	chunk_t *chunk_list = subseg_p->chunk_list, *chunk_p = NULL;
	long long int segment_size = 0;
	
	subseg_p->chunk_list = NULL;
	chunk_list->start_byte = gb->conn[another_index].currentbyte;
	chunk_list->size = chunk_list->last_byte - chunk_list->start_byte + 1;
	delete_subsegment(&segment_p->subsegment[another_index]);
	segment_p = get_first_next_segment_assigned_to_conn(segment_p, another_index);
	delete_finished_head_segment(offset_info);

	while(segment_p != NULL)
	{
		subseg_p = segment_p->subsegment[another_index];
		get_last_chunk(chunk_list)->next = subseg_p->chunk_list;
		subseg_p->chunk_list = NULL;
		delete_subsegment(&segment_p->subsegment[another_index]);
		segment_p = get_first_next_segment_assigned_to_conn(segment_p, another_index);
		delete_finished_head_segment(offset_info);
	}

	chunk_p = chunk_list;
	while(chunk_p != NULL)
	{
		segment_size += chunk_p->size;
		chunk_p = chunk_p->next;
	}
	offset_info->connection[another_index] = add_new_segment(offset_info, 1, chunk_list, segment_size);
}
void print_player_state(int player_state)
{
	printf("%c[%d;%d;%dmCurrent player position: %.2f%c[%d;%dm\n", 
          0x1B, NORMAL, WHITE, BG_BLACK, 
          get_cur_player_pos(),0x1B, WHITE, ATTR_OFF);
  if (player_state == PLAYER_PAUSED)
  	printf("%c[%d;%d;%dmPlayer paused%c[%d;%dm\n", 
            0x1B, NORMAL, WHITE, BG_BLACK,0x1B, WHITE, ATTR_OFF);
  else if (player_state == PLAYER_PLAYING)
  	printf("%c[%d;%d;%dmPlayer playing%c[%d;%dm\n", 
            0x1B, NORMAL, WHITE, BG_BLACK,0x1B, WHITE, ATTR_OFF);
  else 
  	printf("%c[%d;%d;%dmUnknown state of player%c[%d;%dm\n", 
            0x1B, NORMAL, WHITE, BG_BLACK,0x1B, WHITE, ATTR_OFF);
}
void print_recovery_start_msg(void)
{
  printf("%c[%d;%d;%dmRecovery start!%c[%d;%dm\n", 0x1B, BRIGHT, GREEN, BG_BLACK, 0x1B, WHITE, ATTR_OFF);
}
/* return bottle neck portion size in bytes */
long long int get_bottle_neck_size(gb_t *gb, int conn_index)
{
	int another_index = !conn_index;
	segment_t *segment_p, *bottle_neck_seg, *last_segment_assigned;
	long long int bottle_neck_size = 0;

	segment_p = bottle_neck_seg = last_segment_assigned =  gb->offset_info.connection[another_index];
	if(segment_p == NULL)
	{
		printf("get_bottle_neck_size() : another_index error\n");
		//exit(1);
    return 0;
	}
	
	last_segment_assigned = get_first_next_segment_assigned_to_conn(last_segment_assigned, another_index);
	while(last_segment_assigned != NULL)
	{
		if(bottle_neck_seg->subsegment[another_index]->chunk_list->start_byte 
        > last_segment_assigned->subsegment[another_index]->chunk_list->start_byte)
			bottle_neck_seg = last_segment_assigned;
	  last_segment_assigned = get_first_next_segment_assigned_to_conn(last_segment_assigned, another_index);
	}

	if(segment_p == bottle_neck_seg)
		bottle_neck_size = gb->conn[another_index].lastbyte - gb->conn[another_index].currentbyte + 1;
	else
		bottle_neck_size = bottle_neck_seg->subsegment[another_index]->chunk_list->size;
	return bottle_neck_size;
}

int get_behind_conn_index(gb_t *gb)
{
  return (gb->conn[0].currentbyte < gb->conn[1].currentbyte ? 0 : 1);
}

double get_bottle_neck_downloading_time(gb_t *gb, long long int bottle_neck_size)
{
  int bottle_neck_conn_index = gb->conn[0].currentbyte < gb->conn[1].currentbyte ? 0 : 1;
	double downloading_time = (double)bottle_neck_size / (double)gb->conn[bottle_neck_conn_index].bytes_per_second;
	printf("Goodput of links(bytes/s):%lld, %lld\n", gb->conn[0].bytes_per_second, gb->conn[1].bytes_per_second);
	//printf("Bottle neck size : %lld\ttime : %lf\n", bottle_neck_size, downloading_time);
	return downloading_time;
}
void print_recovery_mode(int recovery_mode)
{
  switch (recovery_mode)
  {
    case RECOVERY_FOR_INSUFFICIENT_PLAYABLE_TIME:
    	printf("%c[%d;%d;%dmRecover for insufficient playable time%c[%d;%dm\n", 
            0x1B, BRIGHT, MAGENTA, BG_BLACK,0x1B, WHITE, ATTR_OFF);
      break;
    case RECOVER_FOR_RESUMING_PLAYING:
    	printf("%c[%d;%d;%dmRecover for resuming playing%c[%d;%dm\n", 
            0x1B, BRIGHT, CYAN, BG_BLACK,0x1B, WHITE, ATTR_OFF);
      break;
    default:
    	printf("%c[%d;%d;%dmWARNING: Unrecognized recovery decision!%c[%d;%dm\n", 
            0x1B, BRIGHT, RED, BG_BLACK,0x1B, WHITE, ATTR_OFF);
  }
}

