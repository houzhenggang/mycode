/*
 * arbiter handler
 *
 * Author: junwei.dong
 */
#ifndef __ARBITER_HANDLER_H
#define __ARBITER_HANDLER_H

#include "common.h"

int isArbiterExist();

int do_process_from_arbiter();

#if 0
void do_recv_messgae_from_arbiter();

int init_arbiter_socket_info();

void destroy_arbiter_socket_info();

void process_data_from_arbiter();

void* arbiter_control_handler(void *arg);
#endif

#endif    