/*
 * data_stream
 *
 * Author: junwei.dong
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h> 
 
#include "data_stream.h" 


void insert_stream_list(struct msg_st *mesg, struct list_head *list)
{
    stream_node_t *tmp = NULL;
    tmp = (stream_node_t *)malloc(sizeof(stream_node_t));
    if(NULL != tmp)
    {                
        tmp->data.ip_src = mesg->ip_src;
        tmp->data.port_src = mesg->port_src;
        tmp->data.ip_dst = mesg->ip_dst;
        tmp->data.port_dst = mesg->port_dst;
        tmp->data.proto = mesg->proto;
        tmp->data.proto_mark = mesg->proto_mark;
        tmp->data.up_bytes = mesg->up_bytes;
        tmp->data.down_bytes = mesg->down_bytes;
        tmp->data.duration_time = mesg->duration_time;

        //if sorted_list is empty, then add this node directly!
        //add it before head, means the tail of list
        list_add_tail(&(tmp->list), list);
    }

    return;
}

void print_stream_list()
{
    struct list_head * head = &stream_list_head.list;
    if(list_empty(head))
    {
        return;
    }
    
    stream_node_t *tmp = NULL;
    struct list_head *pos = NULL;
    list_for_each(pos, head)
    {
        tmp = list_entry(pos, stream_node_t, list);

        if(tmp == NULL)
        {
            continue;
        }
        
        printf("%15lu,%15lu,%15lu,%15lu,%15lu,%15lu,%15lu,%15lu,%15lu\n",
              tmp->data.ip_src, tmp->data.port_src, 
              tmp->data.ip_dst , tmp->data.port_dst,
              tmp->data.proto,  tmp->data.proto_mark,
              tmp->data.up_bytes, tmp->data.down_bytes, 
              tmp->data.duration_time);
    }
    
    return;
}

int init_stream_info()
{
    INIT_LIST_HEAD(&stream_list_head.list);
    if(list_empty(&stream_list_head.list))
    {
        marbit_send_log(DEBUG,"stream_list_head is empty!\n");
    }
    else
    {
        marbit_send_log(ERROR,"stream_list_head is not empty!\n");
        return -1;
    }

    return 0;
}
void destroy_stream_info()
{    
    struct list_head *head = &stream_list_head.list;
    if(list_empty(head))
    {
        marbit_send_log(ERROR,"head is empty!\n");
        return;
    }
    
    stream_node_t *tmp = NULL;
    struct list_head *pos = NULL;
    struct list_head *pos_next = NULL;
    
    pos = head->next;
    while(pos != head)
    {
        pos_next = pos->next;
      
        tmp = list_entry(pos, stream_node_t, list);
        
        list_del(pos);

        if(NULL != tmp)
        {
            free(tmp);
        }

        pos = pos_next;
    }
    
    return;
}