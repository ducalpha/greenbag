// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#ifndef _OFFSET_H_
#define _OFFSET_H_
#include <stdlib.h>
#define ONE_SEG 0
#define AHEAD_SEG 1
#define BEHIND_SEG -1
typedef struct
{
	long long int start_byte;		// Start byte offset of chunk, = start byte index
	long long int last_byte;		// Last byte offset of chunk
	long long int size;				// Byte size of chunk
	void *next;
}chunk_t;

typedef struct
{
	chunk_t *chunk_list;			
	long long int size;
}subsegment_t;

typedef struct
{
	int id;							// Identifier of segment
	int recovery_segment;				// General segment : 0, segment for recovery : 1
	chunk_t *chunk_list;
	long long int size;
	subsegment_t *subsegment[2];
	void *next;
}segment_t;

typedef struct offset_info_struct
{
	segment_t *head; // head segment, the first segment
	segment_t *tail; // tail segment, the last segment
	int num_segments;
	int segment_count;
	int completed_segment_count;
	segment_t *connection[2]; // segment which the connection is downloading, maybe not?, found on recovery.c
	long long int init_seg_size;
	int single_link_mode_flag;
	int conn_idle[2]; // mark the connection which is idle; 1: idle, 0: active
	long long int last_seg_offset;
	int fast_conn;
	chunk_t *remaining_chunks;
}offset_info_t;
#include "gb.h"

void add_new_chunk(chunk_t **chunk_list, long long int start_byte, long long int size); 	// Add a new chunk to the tail of the chunk list
void delete_head_chunk(chunk_t **chunk_list);												// Delete the head of the chunk list
segment_t* add_new_segment(offset_info_t *offset_info, int recovery_segment, chunk_t *chunk_list, long long int size); // Add a new segment to the tail of the segment list
int delete_finished_head_segment(offset_info_t *offset_info); 						// Delete the head of the segment list
void add_new_subsegment(struct gb_struct *gb, int conn_index, int target_ratio, int another_ratio, int fixed_subsegment_mode); // Devide the segment into two subsegments
void delete_subsegment(subsegment_t **subseg); 							// Delete subsegment
long long int get_start_byte_next_segment(offset_info_t *offset_info); 				// Get start byte for next segment
long long int get_virtual_seg_size(struct gb_struct *gb, int conn_index, long long int next_seg_size); // calculate virtual segment size
int get_conn_order_case(struct gb_struct *gb, int target_index); // Get case of connection order
chunk_t* get_last_chunk(chunk_t *chunk_list); // Get last chunk of the chunk list
segment_t* get_first_next_segment_assigned_to_conn(segment_t *connection, int conn_index); // Get a segment including one's subsegment
chunk_t* sorting_chunks(chunk_t *chunk_list);	// routine that sorts chunks according to the start byte of chunks
long long int get_remaining_size_for_conn(struct gb_struct *gb, int index);
int is_conn_idle(const offset_info_t *offset_info, int conn_index); // check if the connection is
chunk_t *get_next_chunk_dwnd_by_conn(struct gb_struct *gb, int conn_index);
chunk_t *get_chunk_being_dwnd_by_conn(struct gb_struct *gb, int conn_index);
segment_t *get_segment_being_dwnd_by_conn(struct gb_struct *gb, int conn_index);
chunk_t *get_bn_chunk_info(struct gb_struct *gb, int current_finishing_conn_index,
                                    chunk_t **bn_chunk, long long int *remaining_bn_chunk_size, int *bn_conn_index);
int get_fast_conn_index(struct gb_struct *gb);
void update_last_downloaded_byte(struct gb_struct *gb);
#endif
