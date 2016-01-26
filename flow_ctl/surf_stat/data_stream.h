/*
 * data_stream
 *
 * Author: junwei.dong
 */
#ifndef __DATA_STREAM_H
#define __DATA_STREAM_H

#include "common.h"

void insert_stream_list(struct msg_st *mesg, struct list_head *list);

void print_stream_list();

int init_stream_info();

void destroy_stream_info();

#endif    
