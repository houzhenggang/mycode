/*
 * data_sort 
 *
 * Author: junwei.dong
 */
#ifndef __DATA_SORT_H
#define __DATA_SORT_H

#include "common.h"


void quick_sort(struct list_head * pstHead, struct list_head * low, struct list_head * high);

void do_process_insert_sorted_list(sorted_node_t *sorted_node, 
                                           data_aggregation_class data_agg_class,
                                           sorted_list_condition sorted_list_index,
                                           data_sort_condition data_sort_condition);

void do_process_update_sorted_list(sorted_node_t *sorted_node, 
                                             data_aggregation_class data_agg_class,
                                             sorted_list_condition sorted_list_index,
                                             data_sort_condition data_sort_condition);

void resort_list(sorted_list_condition from_sorted_list_index, 
                           sorted_list_condition to_sorted_list_index,
                           data_sort_condition to_data_sort_condition,
                           data_aggregation_class data_agg_class);

void print_sorted_list(sorted_list_condition sorted_list_index);

int init_sort_info();
void destroy_sort_info();


struct list_head * partition( struct list_head *L, struct list_head * low, struct list_head * high);

int destroyMergelist(merge_sorted_node_t * head);
void MergeSort(merge_sorted_node_t** headRef) ;
void printMergeList(merge_sorted_node_t* head) ;

#endif    