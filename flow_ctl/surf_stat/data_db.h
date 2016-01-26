/*
 * data_db
 *
 * Author: junwei.dong
 */
#ifndef __DATA_DB_H
#define __DATA_DB_H

#include "common.h"

int isNeedReadFromDB(const char *dbname, uint32_t flush_interval);

void do_data_aggregation_from_db(const char* dbname);
void do_data_aggregation_from_file(const char* filename);

void save_to_db(const char *filename, struct list_head *head);

void save_mesg_to_db(const char *filename, struct msg_st *mesg);

#endif