// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#include "offset.h"
#include "gb.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int get_fast_conn_index(struct gb_struct *gb) 
{
  return ((gb->conn[0].bytes_per_second >= gb->conn[1].bytes_per_second) ? 0 : 1);
}

void add_new_chunk(chunk_t **chunk_list, long long int start_byte, long long int size)	// Add a new chunk to the tail of the chunk list.// chunk_list is chunk_list_head
{
	chunk_t *temp = (chunk_t*)malloc(sizeof(chunk_t)), *chunk_p = *chunk_list;
	memset(temp, 0, sizeof(chunk_t));
	temp->start_byte = start_byte;
	temp->last_byte = start_byte + size - 1;
	temp->size = size;
	temp->next = NULL;

	if(*chunk_list == NULL)
		*chunk_list = temp;
	else
	{
		while(chunk_p->next != NULL)
		{
			chunk_p = chunk_p->next;
		}
		chunk_p->next = temp;
	}
}

// Delete the head of the chunk list
void delete_head_chunk(chunk_t **chunk_list)	
{
	chunk_t *temp = *chunk_list;
	*chunk_list = (*chunk_list)->next;
	free(temp);
}

/* Add a new segment to the tail of the segment list */
segment_t* add_new_segment(offset_info_t *offset_info, int recovery_segment, chunk_t *chunk_list, long long int size) 
{
	long long int last_seg_offset = 0;
	segment_t *temp = (segment_t*)malloc(sizeof(segment_t));
	memset(temp, 0, sizeof(segment_t));

	chunk_list = sorting_chunks(chunk_list);

	if(offset_info->num_segments == 0)
		offset_info->head = temp;
	else
		offset_info->tail->next = temp;
	
	offset_info->num_segments++;
	offset_info->segment_count++;
	offset_info->tail = temp;
	
	last_seg_offset = get_last_chunk(chunk_list)->last_byte;
	if(offset_info->last_seg_offset < last_seg_offset)
		offset_info->last_seg_offset = last_seg_offset;

	temp->id = offset_info->segment_count;
	temp->recovery_segment = recovery_segment;
	temp->chunk_list = chunk_list;
	temp->size = size;
	temp->next = NULL;

	return temp;
}
/* 
 * Delete the finished head segment from the segment list
 */
int delete_finished_head_segment(offset_info_t *offset_info)
{
	segment_t *temp = offset_info->head;
	if (temp && temp->subsegment[0] == NULL && temp->subsegment[1] == NULL) {
    while (temp->chunk_list) 
      delete_head_chunk(&temp->chunk_list);

    offset_info->head = offset_info->head->next;
    if (offset_info->head == NULL)
      offset_info->tail = NULL;
    free(temp);

    offset_info->num_segments--;
    offset_info->completed_segment_count++;
    return 1;
  }
  return 0;
}

void add_new_subsegment(struct gb_struct *gb, int conn_index, int target_ratio, int another_ratio, int fixed_subsegment_mode) // Divide the segment into two subsegments
{

  offset_info_t *offset_info = &gb->offset_info;
	segment_t *segment = offset_info->tail;
	chunk_t *seg_chunk = segment->chunk_list;
	subsegment_t *subseg[2];
	long long int chunks_size = 0, subseg_size[2];
	int i = 0, another_index = !conn_index, fast_conn = get_fast_conn_index(gb), slow_conn = !fast_conn;
	
  printf("fast conn, index: %d\tspeed: %lld\n", fast_conn, gb->conn[fast_conn].bytes_per_second);
  printf("slow conn, index: %d\tspeed: %lld\n", slow_conn, gb->conn[slow_conn].bytes_per_second);
	if(seg_chunk == NULL)
	{
		printf("add_new_subsegment() : Segmenting error - No chunks!\n");
		exit(1);
	}

	if(fixed_subsegment_mode)
	{
		subseg_size[conn_index] = segment->size * target_ratio / (target_ratio + another_ratio);
		subseg_size[another_index] = segment->size - subseg_size[conn_index];
	}
	else
	{
		if(segment->recovery_segment == 0)
			subseg_size[conn_index] =
        get_virtual_seg_size(gb, conn_index, gb->offset_info.tail->size) * target_ratio / (target_ratio + another_ratio);
		else
			subseg_size[conn_index] = segment->size * target_ratio / (target_ratio + another_ratio);

		if(subseg_size[conn_index] > segment->size)
			subseg_size[conn_index] = segment->size;
		subseg_size[another_index] = segment->size - subseg_size[conn_index];
	}
	for(i = 0; i < 2; i++)
	{
		if(subseg_size[i] == 0)
			continue;
		segment->subsegment[i] = (subsegment_t *)malloc(sizeof(subsegment_t));
		memset(segment->subsegment[i], 0, sizeof(subsegment_t));
		subseg[i] = segment->subsegment[i];
		subseg[i]->size = subseg_size[i];
	}
	while(seg_chunk != NULL)
	{
		chunks_size += seg_chunk->size;
		if(subseg_size[fast_conn] == 0)
			break;
		if(subseg[fast_conn]->size < chunks_size)
		{
			add_new_chunk(&subseg[fast_conn]->chunk_list, seg_chunk->start_byte, subseg_size[fast_conn]);
			if(seg_chunk->size - subseg_size[fast_conn] != 0)
			{
				add_new_chunk(&subseg[slow_conn]->chunk_list, seg_chunk->start_byte + subseg_size[fast_conn], seg_chunk->size - subseg_size[fast_conn]);
				subseg_size[slow_conn] -= (seg_chunk->size - subseg_size[fast_conn]);
			}
			seg_chunk = seg_chunk->next;
			break;
		}
		add_new_chunk(&subseg[fast_conn]->chunk_list, seg_chunk->start_byte, seg_chunk->size);
		subseg_size[fast_conn] -= seg_chunk->size;
		seg_chunk = seg_chunk->next;
	}
	while(seg_chunk != NULL)
	{
		if(subseg_size[slow_conn] == 0)
		{
			printf("add_new_subsegment() : Segmenting error - Wrong size? or wrong segment chunks\n");
			exit(1);
		}
		add_new_chunk(&subseg[slow_conn]->chunk_list, seg_chunk->start_byte, seg_chunk->size);
		seg_chunk = seg_chunk->next;
	}
}

void delete_subsegment(subsegment_t **subseg_p)
{
	while ((*subseg_p)->chunk_list) {
		delete_head_chunk(&(*subseg_p)->chunk_list);
	}
	free(*subseg_p);
	*subseg_p = NULL;
}

long long int get_start_byte_next_segment(offset_info_t *offset_info) // Get start byte for next segment
{
	if(offset_info->tail == NULL)
	{
		printf("get_start_byte_next_segment - No segments or no chunks!\n");
    assert(0);
		return -1;
	}
	return offset_info->last_seg_offset + 1;
}

/* 
 * return the total remaining chunks of all connection
 */
long long int get_virtual_seg_size(struct gb_struct *gb, int conn_index, long long int next_seg_size)
{
  offset_info_t *offset_info = &gb->offset_info;
	long long int virtual_seg_size = 0;
	segment_t *segment_p = offset_info->head;
	int i = 0;
	if(offset_info->num_segments == 1)
	{
		return segment_p->size;
	}
	else
	{
		for(i = 0 ; i < 2 ; i ++)
		{
			segment_p = offset_info->connection[i];
			if(segment_p == NULL)
				continue;
			if(i != conn_index)
				virtual_seg_size += (gb->conn[i].lastbyte - gb->conn[i].currentbyte + 1);
			while(segment_p->next != NULL)
			{
				segment_p = segment_p->next;
				if(segment_p == offset_info->tail)
					break;
				if(segment_p->subsegment[i] != NULL)
					virtual_seg_size += segment_p->subsegment[i]->size;
			}
		}
		//virtual_seg_size += offset_info->tail->size;
    virtual_seg_size += next_seg_size;
		return virtual_seg_size;
	}
}
int get_conn_order_case(gb_t *gb, int conn_index)
{
  offset_info_t *offset_info = &gb->offset_info;
	int another_index = !conn_index;
  fprintf(stderr, "Currently finishing connection: %d\n", conn_index);
	if (offset_info->num_segments == 1) {
		return ONE_SEG;//0; /* both connections are in the same segment */
  }	else if (offset_info->connection[conn_index] == offset_info->tail) {
		return AHEAD_SEG;//1; /* 2 connections in different segments, conn_index: ahead-segment connection index*/
  }	else if ((offset_info->connection[conn_index] == offset_info->head) || (gb->offset_info.single_link_mode_flag)) {
		return BEHIND_SEG;//-1; /* 2 connections in different segments, conn_index: behine-segment connection index*/
  }	else {
		printf("get_conn_order_case : Wrong case!\n");
    display_offset_info(gb, conn_index);
    //assert(0);
    return BEHIND_SEG;
	}
}

chunk_t* get_last_chunk(chunk_t *chunk_list)
{
	chunk_t *chunk_p = chunk_list;
	while(chunk_p->next != NULL)
		chunk_p = chunk_p->next;
	return chunk_p;
}

segment_t* get_first_next_segment_assigned_to_conn(segment_t *segment_list, int conn_index)
{
	segment_t *segment_p = segment_list;
	while(segment_p->next != NULL)
	{
		segment_p = segment_p->next;
		if(segment_p->subsegment[conn_index] != NULL)
			return segment_p;
	}
	return NULL;
}

chunk_t* sorting_chunks(chunk_t *chunk_list)
{
	chunk_t *sorted_list = NULL, *end = NULL, *chunk_p = chunk_list, *target = chunk_list, *prev = NULL, *next = NULL;

	while(chunk_list != NULL)
	{
		while(chunk_p->next != NULL)
		{
			next = chunk_p->next;
			if(target->start_byte > next->start_byte)
			{
				target = chunk_p->next;
				prev = chunk_p;
			}
			chunk_p = chunk_p->next;
		}

		if(prev == NULL)
			chunk_list = chunk_list->next;
		else
			prev->next = target->next;

		target->next = NULL;

		if(sorted_list == NULL)
			sorted_list = target;
		else
			end->next = target;

		end = target;
		target->next = NULL;
		target = chunk_list;
		chunk_p = chunk_list;
		prev = NULL;
	}
	return sorted_list;
}
long long int get_remaining_size_for_conn(struct gb_struct *gb, int index)
{
  offset_info_t *offset_info = &gb->offset_info;
	segment_t *segment_p = offset_info->connection[index];
	long long int total_size = 0;

	total_size = gb->conn[index].lastbyte - gb->conn[index].currentbyte + 1;
	segment_p = get_first_next_segment_assigned_to_conn(segment_p, index);

	while(segment_p != NULL)
	{
		total_size += segment_p->subsegment[index]->size;
		segment_p = get_first_next_segment_assigned_to_conn(segment_p, index);
	}
	return total_size;
}

int is_conn_idle(const offset_info_t *offset_info, int conn_index)
{
  return (offset_info->connection[conn_index] == NULL); // no segment for this conn?
}

chunk_t *get_next_chunk_dwnd_by_conn(gb_t *gb, int conn_index)
{
  // first chunk that has start_byte > conn_index current_byte
	segment_t *segment_p = gb->offset_info.head;
  while (segment_p) {
    subsegment_t *subsegment_p = segment_p->subsegment[conn_index];
    if (subsegment_p) {
      chunk_t *chunk_p = subsegment_p->chunk_list;
      while (chunk_p) {
        if (chunk_p->start_byte > gb->conn[conn_index].currentbyte)
          return chunk_p;
        chunk_p = chunk_p->next;
      }
    }
    segment_p = segment_p->next;
  }
  return NULL;
}

segment_t *get_segment_being_dwnd_by_conn(gb_t *gb, int conn_index)
{
	segment_t *segment_p = gb->offset_info.head;
	while (segment_p) {
		if (segment_p->subsegment[conn_index] != NULL)
			return segment_p;
		segment_p = segment_p->next;
	}
	return NULL;
  //return gb->offset_info.connection[conn_index]; //hopefully this is updated properly
}

/*
 * return NULL when the conn is idle and not downloading chunk
 */
chunk_t *get_chunk_being_dwnd_by_conn(gb_t *gb, int conn_index)
{
  // find the chunk that has last_byte > conn's currentbyte
  segment_t *being_dwnd_segment = get_segment_being_dwnd_by_conn(gb, conn_index);
  if (being_dwnd_segment) {
    subsegment_t *being_dwnd_subsegment = being_dwnd_segment->subsegment[conn_index];
    assert(being_dwnd_subsegment);
    chunk_t *being_dwnd_chunk = being_dwnd_subsegment->chunk_list;
    while (being_dwnd_chunk) {
      if (being_dwnd_chunk->last_byte > gb->conn[conn_index].currentbyte)
        return being_dwnd_chunk;
      being_dwnd_chunk = being_dwnd_chunk->next;
    }
  }
  return NULL;
}

chunk_t *get_bn_chunk_info(gb_t *gb, int current_finishing_conn_index,
                                    chunk_t **bn_chunk, long long int *remaining_bn_chunk_size, int *bn_conn_index)
{
  int other_conn_index = !current_finishing_conn_index;
  chunk_t *found_bn_chunk = NULL;
  int found_bn_conn_index = 0;
  long long int found_remaining_bn_chunk_size = 0;
  chunk_t *next_chunk_dwnd_by_cur_finishing_conn = NULL;
  chunk_t *chunk_dwnd_by_other_conn = NULL;
  
  next_chunk_dwnd_by_cur_finishing_conn = get_next_chunk_dwnd_by_conn(gb, current_finishing_conn_index);
  chunk_dwnd_by_other_conn = get_chunk_being_dwnd_by_conn(gb, other_conn_index);
  //if (gb->offset_info.single_link_mode_flag) {
  if (is_using_only_one_link(gb)) {
    found_bn_conn_index = current_finishing_conn_index;
    found_bn_chunk = get_chunk_being_dwnd_by_conn(gb, current_finishing_conn_index);
    found_remaining_bn_chunk_size = // can be assumed to be 0
      found_bn_chunk->last_byte - (gb->conn[found_bn_conn_index].currentbyte - 1); 
  } else {
    if ((chunk_dwnd_by_other_conn != NULL) &&
        ((next_chunk_dwnd_by_cur_finishing_conn == NULL) ||
         (next_chunk_dwnd_by_cur_finishing_conn->start_byte > chunk_dwnd_by_other_conn->start_byte))) {
      found_bn_conn_index = other_conn_index;
      found_bn_chunk = chunk_dwnd_by_other_conn;
      found_remaining_bn_chunk_size = // last bn chunk byte - bn conn's completed byte index
        found_bn_chunk->last_byte - (gb->conn[found_bn_conn_index].currentbyte - 1);
    } else {
      found_bn_conn_index = current_finishing_conn_index;
      found_bn_chunk = next_chunk_dwnd_by_cur_finishing_conn;
      found_remaining_bn_chunk_size = next_chunk_dwnd_by_cur_finishing_conn->size;
    }
  }

#ifndef NDEBUG
  printf("\ncurrent finishing conn index: %d\n", current_finishing_conn_index);
  printf("bn chunk start byte: %lld\n", found_bn_chunk->start_byte);
  printf("bn conn index: %d\n", found_bn_conn_index);
  printf("remaining bn chunk size: %lld\n", found_remaining_bn_chunk_size);
#endif

  // return output results
  if (bn_chunk != NULL) *bn_chunk = found_bn_chunk;
  if (remaining_bn_chunk_size != NULL) *remaining_bn_chunk_size = found_remaining_bn_chunk_size;
  if (bn_conn_index != NULL) *bn_conn_index = found_bn_conn_index;
  assert(found_bn_chunk);
  return found_bn_chunk;
}

void update_last_downloaded_byte(gb_t *gb)
{
  if(!gb->ready && gb->run) {
    assert(gb->last_downloaded_byte <= gb->inorder_offset);
    gb->last_downloaded_byte = gb->inorder_offset;
  } else {
    // update last downloaded byte the last time
    gb->last_downloaded_byte = gb->size;
  }
}

