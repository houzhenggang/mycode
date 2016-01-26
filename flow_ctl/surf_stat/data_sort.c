/*
 * data_sort
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
 
#include "data_sort.h" 
	
extern uint32_t g_display_req_line;

//====================================
//===========quick sort ==================
//====================================

sorted_node_t * partion(struct list_head * head, struct list_head * low, struct list_head * high)
{
    sorted_node_t *  pstLow = list_entry(low, sorted_node_t, list);
    sorted_node_t *  pstHigh = list_entry(high, sorted_node_t, list);
    
    aggregation_node_t pivot;
    memcpy(&pivot, &pstLow->data, sizeof(aggregation_node_t));
    
    while ( low != high )
    {
        //从后面往前换
        while ( low != high)
        {
            if(DATA_SORT_CONDITION_CON_NUM == g_data_sort_condition)
            {
                if(pstHigh->data.con_nums < pivot.con_nums)
                {
                    break;
                }
            }
            else if(DATA_SORT_CONDITION_UP_BPS == g_data_sort_condition)
            {
                if(pstHigh->data.up_bps < pivot.up_bps)
                {   
                       break;
                }
            }
            else if(DATA_SORT_CONDITION_DOWN_BPS == g_data_sort_condition)
            {
                if(pstHigh->data.down_bps < pivot.down_bps)
                {   
                       break;
                }
            }
            else if(DATA_SORT_CONDITION_TOTAL_BITS == g_data_sort_condition)
            {
                if(pstHigh->data.total_bits < pivot.total_bits)
                {   
                       break;
                }
            }   
            else if(DATA_SORT_CONDITION_IP_CON_NUM == g_data_sort_condition)
            {
                if(pstHigh->data.ip_con_nums < pivot.ip_con_nums)
                {   
                       break;
                }
            }   
            else if(DATA_SORT_CONDITION_IP_UP_SIZE == g_data_sort_condition)
            {
                if(pstHigh->data.ip_up_size < pivot.ip_up_size)
                {   
                       break;
                }
            }  
            else if(DATA_SORT_CONDITION_IP_DOWN_SIZE == g_data_sort_condition)
            {
                if(pstHigh->data.ip_down_size < pivot.ip_down_size)
                {   
                       break;
                }
            }  
            else if(DATA_SORT_CONDITION_IP_TOTAL_SIZE == g_data_sort_condition)
            {
                if(pstHigh->data.ip_total_size < pivot.ip_total_size)
                {   
                       break;
                }
            }  

            high = high->prev;
            pstHigh = list_entry(high, sorted_node_t, list);
        }

       memcpy(&pstLow->data, &pstHigh->data, sizeof(aggregation_node_t));
        
        
        //从前往后换
        while ( low != high)
        {
            if(DATA_SORT_CONDITION_CON_NUM == g_data_sort_condition)
            {
                if(pstLow->data.con_nums > pivot.con_nums)
                {
                    break;
                }
            }
            else if(DATA_SORT_CONDITION_UP_BPS == g_data_sort_condition)
            {
                if(pstLow->data.up_bps > pivot.up_bps)
                {   
                       break;
                }
            }
            else if(DATA_SORT_CONDITION_DOWN_BPS == g_data_sort_condition)
            {
                if(pstLow->data.down_bps > pivot.down_bps)
                {   
                       break;
                }
            }
            else if(DATA_SORT_CONDITION_TOTAL_BITS == g_data_sort_condition)
            {
                if(pstLow->data.total_bits > pivot.total_bits)
                {   
                       break;
                }
            }   
            else if(DATA_SORT_CONDITION_IP_CON_NUM == g_data_sort_condition)
            {
                if(pstLow->data.ip_con_nums > pivot.ip_con_nums)
                {   
                       break;
                }
            }   
            else if(DATA_SORT_CONDITION_IP_UP_SIZE == g_data_sort_condition)
            {
                if(pstLow->data.ip_up_size > pivot.ip_up_size)
                {   
                       break;
                }
            }  
            else if(DATA_SORT_CONDITION_IP_DOWN_SIZE == g_data_sort_condition)
            {
                if(pstLow->data.ip_down_size > pivot.ip_down_size)
                {   
                       break;
                }
            }  
            else if(DATA_SORT_CONDITION_IP_TOTAL_SIZE == g_data_sort_condition)
            {
                if(pstLow->data.ip_total_size > pivot.ip_total_size)
                {   
                       break;
                }
            }  

            
            low = low->next;
            pstLow = list_entry(low, sorted_node_t, list);
        }

        memcpy(&pstHigh->data, &pstLow->data, sizeof(aggregation_node_t));
    }
 

     memcpy(&pstLow->data, &pivot, sizeof(aggregation_node_t));
    
    return pstLow;
}

//快排
void quick_sort(struct list_head * head, struct list_head * low, struct list_head * high)
{
   // struct list_head * head = &sorted_list_arry[SORTED_BASE_AGG].list;
    if(list_empty(head))
    {
        return;
    }

    sorted_node_t *  pstLow = list_entry(low, sorted_node_t, list);
    sorted_node_t *  pstHigh = list_entry(high, sorted_node_t, list);
    
    sorted_node_t * pstTmp = NULL;
    
    pstTmp = partion(head, low, high);

    if ( pstLow != pstTmp )
    {
            quick_sort(head, low, (&pstTmp->list)->prev);
    }
    
    if ( pstHigh != pstTmp )
    {
            quick_sort(head, (&pstTmp->list)->next, high);
    }
 
}

//====================================
//===========insert sort ==================
//====================================

#if 1
void resort_list(sorted_list_condition from_sorted_list_index, 
                           sorted_list_condition to_sorted_list_index,
                           data_sort_condition to_data_sort_condition,
                           data_aggregation_class data_agg_class)
{
    struct list_head *from_head = &sorted_list_arry[from_sorted_list_index].list; 
    struct list_head *to_head = &sorted_list_arry[to_sorted_list_index].list; 
        
    sorted_node_t *tmp;
    struct list_head *first = from_head->next; 
    struct list_head *last = from_head->prev;
    struct list_head *pos;
    struct list_head *pos_next;
    
    //g_data_agg_class = DATA_AGG_CLASS_APP;
    //g_data_sort_condition = to_data_sort_condition;

    int count = 0;
    pos = first;
    while(pos != last)
   //list_for_each(pos, &appid_sorted_list_head.list)
    {
        pos_next = pos->next;
      
        tmp = list_entry(pos, sorted_node_t, list);
#if 0
        __list_del(pos->prev, pos->next);
        (&tmp->list)->next = NULL;
        (&tmp->list)->prev = NULL;
#else
        list_del(pos);
#endif
        pos = pos_next;

        do_process_insert_sorted_list(tmp, data_agg_class, to_sorted_list_index, to_data_sort_condition);

        count++;
    }

    

  //  list_for_each(pos, to_head)
   // {
     //   tmp = list_entry(pos, sorted_node_t, list);

      //  printf("appid[%u] con_nums[%u] up_bps[%u] down_bps[%u] total_bits[%u]\n", 
         //       tmp->appid, tmp->con_nums, tmp->up_bps, tmp->down_bps, tmp->total_bits);
   // }

  marbit_send_log(INFO,"after resort count[%d]:\n", count);

}
#endif
void do_process_update_sorted_list(sorted_node_t *sorted_node,
                                             data_aggregation_class data_agg_class,
                                             sorted_list_condition sorted_list_index,
                                             data_sort_condition data_sort_condition)
{
    struct list_head *head = &sorted_list_arry[sorted_list_index].list;

     //if sorted_list is empty, then return directly!
     if(list_empty(head) || NULL == sorted_node)
     {
        marbit_send_log(DEBUG, "head is empty\n");
        return;
    }

    //if it's the only one node, then keep it
    sorted_node_t *need_update_node =  sorted_node;
    if((&need_update_node->list)->prev == head &&
        (&need_update_node->list)->next == head)
        
    {
        marbit_send_log(DEBUG,"this node is the first node, so keep it!\n");
        return;
    }

    //compare with all the next node
    struct list_head *find_list = (&need_update_node->list)->next;
    sorted_node_t *find_tmp = list_entry(find_list, sorted_node_t, list);
    //sorted_node_t *find_tmp = list_entry(head->next, sorted_node_t, list);
    
     list_for_each_entry_from(find_tmp, head, list)
     {
      //  if(NULL == find_tmp)
     //   {
        //    marbit_send_log(DEBUG, "find_tmp is NULL\n");
         //   continue;
      //  }
        
        if(DATA_SORT_CONDITION_CON_NUM == data_sort_condition)
         {
                if(need_update_node->data.con_nums < find_tmp->data.con_nums)
                {
                    break;
                }
        }
        else if(DATA_SORT_CONDITION_UP_BPS == data_sort_condition)
        {
            if(need_update_node->data.up_bps < find_tmp->data.up_bps)
            {
                break;
            }
        }
        else if(DATA_SORT_CONDITION_DOWN_BPS == data_sort_condition)
        {
            if(need_update_node->data.down_bps < find_tmp->data.down_bps)
            {   
                   break;
            }
        }
        else if(DATA_SORT_CONDITION_TOTAL_BITS == data_sort_condition)
        {
            if(need_update_node->data.total_bits < find_tmp->data.total_bits)
            {   
                   break;
            }
        } 
        else if(DATA_SORT_CONDITION_IP_CON_NUM == data_sort_condition)
        {
            if(need_update_node->data.ip_con_nums < find_tmp->data.ip_con_nums)
            {   
                   break;
            }
        }  
     }
  

    if(&find_tmp->list == head)
    {
        //if it's the last one, and it's the biggest
        if( (&need_update_node->list == head->prev) && 
             (&find_tmp->list == head))
        {
            //not need to move, just keep it
        }
        
        //if it's the first one or in middle, and it's the biggest
        else// if((&need_update_node->list == head->next) && (&find_tmp->list == head))
        {
            //should move it
            list_del(&need_update_node->list);
            list_add_tail(&need_update_node->list, head); 
        }
    }
    else
    {
        //if samller than the next node, and the next node is not head
        if(&find_tmp->list == (&need_update_node->list)->next)
        {
            //printf("keep current!\n");
        }
        
        else
        {
            //should move it
            list_del(&need_update_node->list);
            list_add_tail(&need_update_node->list, &find_tmp->list); 
        }
    }
    

    return;
}

extern int32_t g_data_agg_class_condition_appid;

void do_process_insert_sorted_list(sorted_node_t *sorted_node, 
                                         data_aggregation_class data_agg_class,
                                         sorted_list_condition sorted_list_index,
                                         data_sort_condition data_sort_condition)
{       
    struct list_head *head = &sorted_list_arry[sorted_list_index].list;

    //if sorted_list is empty, then add this node directly!
    if(list_empty(head))
    {
        list_add(&(sorted_node->list), head);
        return;
    }

    //if sorted_list is not empty, then use algorithm to insert
    //iterate over list from first to last
    sorted_node_t *find_pos = NULL;
    list_for_each_entry(find_pos,  head, list)
    {
        if(NULL == find_pos)
        {
            continue;
        }
        
        //find one element is more bigger
        if(DATA_SORT_CONDITION_CON_NUM == data_sort_condition)
        {
                if(sorted_node->data.con_nums < find_pos->data.con_nums)
                {   
                       break;
                } 
        }
        else if(DATA_SORT_CONDITION_UP_BPS == data_sort_condition)
        {
            if(sorted_node->data.up_bps < find_pos->data.up_bps)
            {   
                   break;
            }
        }
        else if(DATA_SORT_CONDITION_DOWN_BPS == data_sort_condition)
        {
            if(sorted_node->data.down_bps < find_pos->data.down_bps)
            {   
                   break;
            }
        }
        else if(DATA_SORT_CONDITION_TOTAL_BITS == data_sort_condition)
        {
            if(sorted_node->data.total_bits < find_pos->data.total_bits)
            {   
                   break;
            }
        }   
        else if(DATA_SORT_CONDITION_IP_CON_NUM == data_sort_condition)
        {
            if(sorted_node->data.ip_con_nums < find_pos->data.ip_con_nums)
            {   
                   break;
            }
        }   
        else if(DATA_SORT_CONDITION_IP_UP_SIZE == data_sort_condition)
        {
            if(sorted_node->data.ip_up_size < find_pos->data.ip_up_size)
            {   
                   break;
            }
        }  
        else if(DATA_SORT_CONDITION_IP_DOWN_SIZE == data_sort_condition)
        {
            if(sorted_node->data.ip_down_size < find_pos->data.ip_down_size)
            {   
                   break;
            }
        }  
        else if(DATA_SORT_CONDITION_IP_TOTAL_SIZE == data_sort_condition)
        {
            if(sorted_node->data.ip_total_size < find_pos->data.ip_total_size)
            {   
                   break;
            }
        }  
       // else if(DATA_SORT_CONDITION_DOWN_SIZE == data_sort_condition)
        //{
          //  if(sorted_node->data.link_down_size < find_pos->data.link_down_size)
          //  {   
             //      break;
           // }
      //  } 
    }

    //insert it before find_pos
    list_add_tail(&(sorted_node->list), &(find_pos->list));    


    return;
}

extern uint32_t PRINT_STAT_RESULT;
extern uint32_t g_display_req_line; //web request display count
extern uint32_t g_display_req_begin_line; //web request display begin index
extern uint32_t g_display_req_end_line; //web request display end index
extern uint32_t g_display_res_line;
void print_sorted_list(sorted_list_condition sorted_list_index)
{
    struct list_head *head = &sorted_list_arry[sorted_list_index].list;
    if(list_empty(head))
    {
        marbit_send_log(DEBUG, "head is empty\n");
        return;
    }

    
    FILE *fp = NULL; 
    
    if(1 == PRINT_STAT_RESULT)
    {
        if(SORTED_BASE_APP == g_sorted_list_index)
        {
            fp = fopen(DATA_BASEAPP_FILE_NAME_RESULT,"w+");
            if(NULL == fp)
            {
                marbit_send_log(INFO,"failed to open file %s\n", DATA_BASEAPP_FILE_NAME_RESULT);
                return;
            }
        }
        else if(SORTED_BASE_IP == g_sorted_list_index)
        {
            fp = fopen(DATA_BASEIP_FILE_NAME_RESULT,"w+");
            if(NULL == fp)
            {
                marbit_send_log(INFO,"failed to open file %s\n", DATA_BASEIP_FILE_NAME_RESULT);
                return;
            }
        }
        else if(SORTED_BASE_LINK == g_sorted_list_index)
        {
            fp = fopen(DATA_BASELINK_FILE_NAME_RESULT,"w+");
            if(NULL == fp)
            {
                marbit_send_log(INFO,"failed to open file %s\n", DATA_BASELINK_FILE_NAME_RESULT);
                return;
            }
        }
    }
    
    
    uint32_t display_index = 0;
    
    char buf[512];
    memset(buf, 0, 512);

    sorted_node_t *tmp = NULL;
    struct list_head *pos = NULL;
    
        
    list_for_each_prev(pos, head)
    {
        if(G_DISPLAY_LINE_DEFAULT != g_display_req_line )
        {
            display_index++;
            if(display_index < g_display_req_begin_line)
            {
                continue;
            }

            if(display_index > g_display_req_end_line)
            {
                break;
            }
        }
        
        /* 
          * 此刻：
          * pos->next指向了下一项的list变量，而pos->prev指向上一项的list变量。
          * 而每项都是sorted_node_t类型。
          * 但是，我们需要访问的是这些项，而不是项中的list变量。
          * 因此需要调用list_entry()宏。
          * 
          * 给定指向struct list_head的指针，它所属的宿主数据结构的类型，
          * 以及它在宿主数据结构中的名称，
          * list_entry返回指向宿主数据结构的指针。
          * 
          * 例如，在下面一行， list_entry()返回指向pos所属sorted_node_t项的指针。
        */
        tmp = list_entry(pos, sorted_node_t, list);
        if(NULL == tmp)
        {
            continue;
        }   

         memset(buf, 0, 512);
         if(SORTED_BASE_APP == g_sorted_list_index)
         {
            sprintf(buf, "%-15lu | %-15lu | %-15lu | %-15lu | %-15lu\n", 
                    tmp->data.appid,  tmp->data.con_nums, tmp->data.up_bps, tmp->data.down_bps, tmp->data.total_bits);

            //printf("%-15lu | %-15lu | %-15lu | %-15lu | %-15lu\n", 
                 //    tmp->data.appid,  tmp->data.con_nums, tmp->data.up_bps, tmp->data.down_bps, tmp->data.total_bits);
            
             if(1 == PRINT_STAT_RESULT)
             {
                fprintf(fp,"%-15lu | %-15lu | %-15lu | %-15lu | %-15lu\n", 
                     tmp->data.appid,  tmp->data.con_nums, tmp->data.up_bps, tmp->data.down_bps, tmp->data.total_bits);
             }
         }
         else if(SORTED_BASE_IP == g_sorted_list_index)
         {
            sprintf(buf, "%-15lu | %-15lu | %-15lu | %-15lu | %-15lu\n", 
                     tmp->data.ip,  tmp->data.ip_con_nums, tmp->data.ip_up_size, tmp->data.ip_down_size, tmp->data.ip_total_size);

           // printf("%-15lu | %-15lu | %-15lu | %-15lu | %-15lu\n", 
                 //    tmp->data.ip,  tmp->data.ip_con_nums, tmp->data.ip_up_size, tmp->data.ip_down_size, tmp->data.ip_total_size);
           
            if(1 == PRINT_STAT_RESULT)
             {
                 fprintf(fp,"%-15lu | %-15lu | %-15lu | %-15lu | %-15lu\n", 
                     tmp->data.ip,  tmp->data.ip_con_nums, tmp->data.ip_up_size, tmp->data.ip_down_size, tmp->data.ip_total_size);
             }
         }
         else if(SORTED_BASE_LINK == g_sorted_list_index)
         {
            if(IPPROTO_TCP == tmp->data.link_proto)
            {
                sprintf(buf, "%-15lu | %-15s | %-80s | %-15lu | %-15lu |%-15lu\n", 
                     tmp->data.link_app,  IPPROTO_TCP_NAME, tmp->data.link_con_info, tmp->data.link_duration_time, tmp->data.link_up_size, tmp->data.link_down_size);

                // printf("%-15lu | %-15s | %-80s | %-15lu | %-15lu |%-15lu\n", 
                   //  tmp->data.link_app,  IPPROTO_TCP_NAME, tmp->data.link_con_info, tmp->data.link_duration_time, tmp->data.link_up_size, tmp->data.link_down_size);

                if(1 == PRINT_STAT_RESULT)
                {
                     fprintf(fp,"%-15lu | %-15s | %-80s | %-15lu | %-15lu |%-15lu\n", 
                     tmp->data.link_app,  IPPROTO_TCP_NAME, tmp->data.link_con_info, tmp->data.link_duration_time, tmp->data.link_up_size, tmp->data.link_down_size);
                }
            }
            else if(IPPROTO_UDP == tmp->data.link_proto)
            {
                sprintf(buf, "%-15lu | %-15s | %-80s | %-15lu | %-15lu |%-15lu\n", 
                     tmp->data.link_app,  IPPROTO_UDP_NAME, tmp->data.link_con_info, tmp->data.link_duration_time, tmp->data.link_up_size, tmp->data.link_down_size);
                //printf("%-15lu | %-15s | %-80s | %-15lu | %-15lu |%-15lu\n", 
                  //   tmp->data.link_app,  IPPROTO_UDP_NAME, tmp->data.link_con_info, tmp->data.link_duration_time, tmp->data.link_up_size, tmp->data.link_down_size);

                if(1 == PRINT_STAT_RESULT)
                {
                     fprintf(fp,"%-15lu | %-15s | %-80s | %-15lu | %-15lu |%-15lu\n", 
                     tmp->data.link_app,  IPPROTO_UDP_NAME, tmp->data.link_con_info, tmp->data.link_duration_time, tmp->data.link_up_size, tmp->data.link_down_size);

                }
            }
         }
        
        printf(buf);
    }

    printf("%-15lu\n", g_display_res_line);

    if(1 == PRINT_STAT_RESULT)
    {
        fclose(fp);
    }
    
    
    return;
}

int init_sort_info()
{
    sorted_list_condition i = SORTED_LIST_NOT_USED;
    for(i= SORTED_LIST_NOT_USED+1; i < SORTED_LIST_MAX; i++)
    {
        INIT_LIST_HEAD(&sorted_list_arry[i].list);
    }

    INIT_LIST_HEAD(&appid_sorted_list_head.list);
    
    return 0;
}
void destroy_sort_info()
{   
    sorted_node_t *tmp = NULL;
    struct list_head *pos = NULL;
    struct list_head *pos_next = NULL;
    struct list_head *first = NULL;
    struct list_head *head = NULL;

    sorted_list_condition i = SORTED_LIST_NOT_USED;
    for(i= SORTED_LIST_NOT_USED+1; i < SORTED_LIST_MAX; i++)
    {
        head = &sorted_list_arry[i].list;
        
        if(list_empty(head))
        {
            continue;
        }

        pos = head->next;
        while(pos != head)
        {
            pos_next = pos->next;
          
            tmp = list_entry(pos, sorted_node_t, list);

            list_del(pos);

            if(NULL != tmp)
            {
                free(tmp);
            }
            pos = pos_next;
        }
        
    }

    return;
}

//==================================================
//=========== merge sort ===============================
//==================================================
extern merge_sorted_node_t* merglist;
void printMergeList(merge_sorted_node_t* head)  
{  
    char buf[512];
    memset(buf, 0, 512);
    uint32_t display_index = 0; 
    merge_sorted_node_t* node = head;
    
    while( node != NULL )  
    {   
        if(G_DISPLAY_LINE_DEFAULT != g_display_req_line )
        {
            display_index++;

            if(display_index < g_display_req_begin_line)
            {
                node = node->next;  
                continue;
            }

            if(display_index > g_display_req_end_line)
            {
                break;
            }
        }
     
        memset(buf, 0, 512);
         if(SORTED_BASE_APP == g_sorted_list_index)
         {
            sprintf(buf, "%-15lu | %-15lu | %-15lu | %-15lu | %-15lu\n", 
                    node->data.appid,  node->data.con_nums, node->data.up_bps, node->data.down_bps, node->data.total_bits);
         }
         else if(SORTED_BASE_IP == g_sorted_list_index)
         {
            sprintf(buf, "%-15lu | %-15lu | %-15lu | %-15lu | %-15lu\n", 
                     node->data.ip,  node->data.ip_con_nums, node->data.ip_up_size, node->data.ip_down_size, node->data.ip_total_size);
         }
         
         printf(buf);
         
          node = node->next;  
    }  
    printf("%-15lu\n", g_display_res_line);    
    
}  

  
/* UTILITY FUNCTIONS */  
/* Split the nodes of the given list into front and back halves, 
    and return the two lists using the references parameters. 
    If the length is odd, the extra node shold go in the front list. 
    Uses the fast/slow pointer strategy. */  
void FrontBackSplit(merge_sorted_node_t* source, merge_sorted_node_t** frontRef, merge_sorted_node_t** backRef)  
{  
    merge_sorted_node_t* fast;  
    merge_sorted_node_t* slow;  
  
    if(source == NULL || source->next == NULL)  
    {  
        *frontRef = source;  
        *backRef = NULL;  
    }  
    else  
    {  
        slow = source;  
        fast = source->next;  
  
        /* Advance 'fast' two nodes, and advance 'slow' one node */   
        while(fast != NULL)  
        {  
            fast = fast->next;  
            if( fast != NULL )  
            {  
                slow = slow->next;  
                fast = fast->next;  
            }  
        }  
  
        *frontRef = source;  
        *backRef = slow->next;  
        slow->next = NULL;  
    }  
}  


merge_sorted_node_t* SortedMerge(merge_sorted_node_t* a, merge_sorted_node_t* b)  
{  
    merge_sorted_node_t* result = NULL;  
    uint64_t a_value = 0; 
    uint64_t b_value = 0; 
  
    /* Base cases */  
    if(a == NULL)  
        return (b);  
    else if(b == NULL)  
        return (a);  
  
    /* Pick either a or b recur   
      * if(a->data.con_nums <= b->data.con_nums)  //from small to bigger
      * if(a->data.con_nums > b->data.con_nums)   // from bigger to small
      */
    if(DATA_SORT_CONDITION_CON_NUM == g_data_sort_condition)
    {
            a_value = a->data.con_nums;
            b_value = b->data.con_nums;
    }
    else if(DATA_SORT_CONDITION_UP_BPS == g_data_sort_condition)
    {
        a_value = a->data.up_bps;
        b_value = b->data.up_bps;
    }
    else if(DATA_SORT_CONDITION_DOWN_BPS == g_data_sort_condition)
    {
        a_value = a->data.down_bps;
        b_value = b->data.down_bps;
    }
    else if(DATA_SORT_CONDITION_TOTAL_BITS == g_data_sort_condition)
    {
        a_value = a->data.total_bits;
        b_value = b->data.total_bits;
    }   
    else if(DATA_SORT_CONDITION_IP_CON_NUM == g_data_sort_condition)
    {
        a_value = a->data.ip_con_nums;
        b_value = b->data.ip_con_nums;
    }   
    else if(DATA_SORT_CONDITION_IP_UP_SIZE == g_data_sort_condition)
    {
        a_value = a->data.ip_up_size;
        b_value = b->data.ip_up_size;
    }  
    else if(DATA_SORT_CONDITION_IP_DOWN_SIZE == g_data_sort_condition)
    {
        a_value = a->data.ip_down_size;
        b_value = b->data.ip_down_size;
    }  
    else if(DATA_SORT_CONDITION_IP_TOTAL_SIZE == g_data_sort_condition)
    {
        a_value = a->data.ip_total_size;
        b_value = b->data.ip_total_size;
    }  
    
    if(a_value > b_value)
    {  
        result = a;  
        result->next = SortedMerge(a->next, b);  
    }  
    else  
    {  
        result = b;  
        result->next = SortedMerge(a, b->next);     
    }  
    return (result);  
}  

/*sorts the linked list by changing next pointers(not data) */  
void MergeSort(merge_sorted_node_t** headRef)  
{  
    merge_sorted_node_t* head = *headRef;  
    
    merge_sorted_node_t* a;  
    merge_sorted_node_t* b;  
  
    /*base case-- length 0 or 1 */  
    if((head == NULL) || (head->next == NULL))  
    {  
        return;  
    }     
    /*Split head into 'a' and 'b' sublists */  
    FrontBackSplit(head, &a, &b);  
  
    /*Recursively sort the sublists */  
    MergeSort(&a);  
    MergeSort(&b);  
  
    /* answer = merge the two sorted lists together */  
    *headRef = SortedMerge(a, b);  
}  
  



int destroyMergelist(merge_sorted_node_t * head)
{
    merge_sorted_node_t * tmp = NULL;   /* Temp pointer for circle  */
    int ret = 0;
    if ( !head )
    {
       marbit_send_log(INFO,"pstHead is null\n");
        ret = -1;
    }
    else
    {
        while ( head )  /* Free  nodes      */
        {
            tmp = head;
            head = head->next;
            free(tmp);
        }
        head = NULL;
    }
    return ret;
}
void initMergeList(merge_sorted_node_t** head_ref)  
{  
    int iCir = 0;
    merge_sorted_node_t * pstTmp1 = NULL;
    merge_sorted_node_t * pstTmp2 = NULL;

    merglist = (merge_sorted_node_t *)malloc(sizeof(merge_sorted_node_t));
    if(NULL == merglist)
    {
        marbit_send_log(ERROR,"NO_MEMORY\n");
    }
    merglist->prev = NULL;
    merglist->next = NULL;
    
    pstTmp1 = merglist;
    
    int imax = 50000;
    for( iCir = 0; iCir < imax; iCir++ )
    {
        pstTmp2 = (merge_sorted_node_t *)malloc(sizeof(merge_sorted_node_t));
        if ( !pstTmp2 )
        {
            marbit_send_log(ERROR,"NO_MEMORY\n");
        }
        pstTmp2->data.con_nums = imax - iCir;
        pstTmp2->next = NULL;
        pstTmp1->next = pstTmp2;
        pstTmp2->prev = pstTmp1;
        pstTmp1 = pstTmp2;
    } 
}  

