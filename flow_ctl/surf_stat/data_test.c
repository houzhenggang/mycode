/*
 * data_test
 *
 * Author: junwei.dong
 */
 
#include "data_test.h" 
#include "data_stream.h" 
#include "data_sort.h" 

extern uint32_t g_baselink_ip;
extern uint32_t g_display_req_line; //defalut is 0
extern uint32_t g_flush_interval; //seconds, default is 0
extern int32_t  g_data_agg_class_condition_appid;
extern uint32_t g_baselink_ip;


sorted_node_t  test_list_head;
extern merge_sorted_node_t* merglist;

void test_merge_sort()
{
     initMergeList(&merglist);

    marbit_send_log(INFO,"\n before Sorted Linked List is: \n");  
    printMergeList(merglist);

    MergeSort(&merglist);  

    marbit_send_log(INFO,"\n after Sorted Linked List is: \n");  
    printMergeList(merglist);

    destroyMergelist(merglist);
}

int initDuList()
{
 
    int iRet = 0;

    INIT_LIST_HEAD(&test_list_head.list);
    
    sorted_node_t * pstTmp = NULL;
    int iCir = 0;
    int imax = 50000;
    srand( time(NULL) );//随机数 
    
    for( iCir = 0; iCir < imax; iCir++ )
    {
        pstTmp = (sorted_node_t *)malloc(sizeof(sorted_node_t));
        if ( !pstTmp )
        {
            marbit_send_log(ERROR,"NO_MEMORY\n");
            break;
        }
        //赋初值
        pstTmp->data.con_nums = rand() %50000;//imax - iCir;


#if 0
        if(iCir == 101) pstTmp->data.con_nums = 4294967150;
        else if(iCir == 102)  pstTmp->data.con_nums =945776151;
        else if(iCir == 103)  pstTmp->data.con_nums =592968387; 
        else if(iCir == 104)  pstTmp->data.con_nums =2802214688;
        else if(iCir == 105)  pstTmp->data.con_nums =3195198864;
        else if(iCir == 106)  pstTmp->data.con_nums = 767192818 ;
        else if(iCir == 107)  pstTmp->data.con_nums = 2232450973;                                         
        else if(iCir == 108)  pstTmp->data.con_nums = 3770929166;                                        
        else if(iCir == 109)  pstTmp->data.con_nums = 3008266200;                                             
        else if(iCir == 110)  pstTmp->data.con_nums = 2575673444;                                        
        else if(iCir == 111)  pstTmp->data.con_nums = 2041222669;                                             
        else if(iCir == 112)  pstTmp->data.con_nums = 1733079149;                                             
        else if(iCir == 113)  pstTmp->data.con_nums = 1706396588;                                             
        else if(iCir == 114)  pstTmp->data.con_nums = 1667486937;                                             
        else if(iCir == 115)  pstTmp->data.con_nums = 1303662380;                                             
        else if(iCir == 116)  pstTmp->data.con_nums = 1071619032;                                             
        else if(iCir == 117)  pstTmp->data.con_nums = 1057764485;                                             
        else if(iCir == 118)  pstTmp->data.con_nums = 1019279782;                                             
        else if(iCir == 119)  pstTmp->data.con_nums = 924026884;                                              
        else if(iCir == 120)  pstTmp->data.con_nums = 812833550;                                              
        else if(iCir == 121)  pstTmp->data.con_nums = 780308963;                                              
        else if(iCir == 122)  pstTmp->data.con_nums = 744289013;                                              
        else if(iCir == 123)  pstTmp->data.con_nums = 738445256;                                              
        else if(iCir == 124)  pstTmp->data.con_nums = 660725094;                                              
        else if(iCir == 125)  pstTmp->data.con_nums = 631994020;                                              
        else if(iCir == 126)  pstTmp->data.con_nums = 468045584;                                              
        else if(iCir == 127)  pstTmp->data.con_nums = 328372140;                                              
        else if(iCir == 128)  pstTmp->data.con_nums = 325921183;                                              
        else if(iCir == 129)  pstTmp->data.con_nums = 246931346;                                              
        else if(iCir == 130)  pstTmp->data.con_nums = 215191498;                                              
        else if(iCir == 131)  pstTmp->data.con_nums = 214458546;                                              
        else if(iCir == 132)  pstTmp->data.con_nums = 189779728;                                              
        else if(iCir == 133)  pstTmp->data.con_nums = 184465890;                                              
        else if(iCir == 134)  pstTmp->data.con_nums = 164926138;                                              
        else if(iCir == 135)  pstTmp->data.con_nums = 118672626;                                              
        else if(iCir == 136)  pstTmp->data.con_nums = 106939034;                                             
        else if(iCir == 137)  pstTmp->data.con_nums = 94181158;                                               
        else if(iCir == 138)  pstTmp->data.con_nums = 80231381;                                               
        else if(iCir == 139)  pstTmp->data.con_nums = 58457598;                                               
        else if(iCir == 140)  pstTmp->data.con_nums = 58426110;                                               
        else if(iCir == 141)  pstTmp->data.con_nums = 56694621;                                               
        else if(iCir == 142)  pstTmp->data.con_nums = 52375939;                                               
        else if(iCir == 143)  pstTmp->data.con_nums = 46443430;                                               
        else if(iCir == 144)  pstTmp->data.con_nums = 41321155;                                               
        else if(iCir == 145)  pstTmp->data.con_nums = 41023270;                                               
        else if(iCir == 146)  pstTmp->data.con_nums = 36277250;                                               
        else if(iCir == 147)  pstTmp->data.con_nums = 33359111;                                               
        else if(iCir == 148)  pstTmp->data.con_nums = 33005213;                                               
        else if(iCir == 149)  pstTmp->data.con_nums = 32653851;                                               
        else if(iCir == 150)  pstTmp->data.con_nums = 30340033;                                               
        else if(iCir == 151)  pstTmp->data.con_nums = 28495727;                                               
        else if(iCir == 152)  pstTmp->data.con_nums = 27422127;                                               
        else if(iCir == 153)  pstTmp->data.con_nums = 26197118;                                               
        else if(iCir == 154)  pstTmp->data.con_nums = 25827366;                                               
        else if(iCir == 155)  pstTmp->data.con_nums = 25129648;                                               
        else if(iCir == 156)  pstTmp->data.con_nums = 23679421;                                               
        else if(iCir == 157)  pstTmp->data.con_nums = 23519850;                                               
        else if(iCir == 158)  pstTmp->data.con_nums = 22122468;                                               
        else if(iCir == 159)  pstTmp->data.con_nums = 21021014;                                               
        else if(iCir == 160)  pstTmp->data.con_nums = 20310293;                                               
        else if(iCir == 161)  pstTmp->data.con_nums = 13925185;                                               
        else if(iCir == 162)  pstTmp->data.con_nums = 13670944;                                               
        else if(iCir == 163)  pstTmp->data.con_nums = 13245524;                                               
        else if(iCir == 164)  pstTmp->data.con_nums = 12601063;                                               
        else if(iCir == 165)  pstTmp->data.con_nums = 12014880;                                               
        else if(iCir == 166)  pstTmp->data.con_nums = 11986681;                                               
        else if(iCir == 167)  pstTmp->data.con_nums = 11401731;                                               
        else if(iCir == 168)  pstTmp->data.con_nums = 11094662;                                               
        else if(iCir == 169)  pstTmp->data.con_nums = 10995021;                                               
        else if(iCir == 170)  pstTmp->data.con_nums = 9855589;                                                
        else if(iCir == 171)  pstTmp->data.con_nums = 9832749;                                                
        else if(iCir == 172)  pstTmp->data.con_nums = 8699778;                                                
        else if(iCir == 173)  pstTmp->data.con_nums = 8291202;                                                
        else if(iCir == 174)  pstTmp->data.con_nums = 7443947;                                                
        else if(iCir == 175)  pstTmp->data.con_nums = 6882847;                                                
        else if(iCir == 176)  pstTmp->data.con_nums = 6360214;                                                
        else if(iCir == 177)  pstTmp->data.con_nums = 4677850;                                                
        else if(iCir == 178)  pstTmp->data.con_nums = 3943560;                                                
        else if(iCir == 179)  pstTmp->data.con_nums = 3208634;                                                
        else if(iCir == 180)  pstTmp->data.con_nums = 3120219;                                                
        else if(iCir == 181)  pstTmp->data.con_nums = 3100300;                                                
        else if(iCir == 182)  pstTmp->data.con_nums = 2947383;                                                
        else if(iCir == 183)  pstTmp->data.con_nums = 2928644;                                                
        else if(iCir == 184)  pstTmp->data.con_nums = 2920899;                                                
        else if(iCir == 185)  pstTmp->data.con_nums = 2386528;                                                
        else if(iCir == 186)  pstTmp->data.con_nums = 1703038;                                                
        else if(iCir == 187)  pstTmp->data.con_nums = 1528478;                                                
        else if(iCir == 188)  pstTmp->data.con_nums = 1448387;                                                
        else if(iCir == 189)  pstTmp->data.con_nums = 1365641;                                                
        else if(iCir == 190)  pstTmp->data.con_nums = 1331375;                                                
        else if(iCir == 191)  pstTmp->data.con_nums = 1203481;                                                
        else if(iCir == 192)  pstTmp->data.con_nums = 1188901;                                                
        else if(iCir == 193)  pstTmp->data.con_nums = 1123100;                                                
        else if(iCir == 194)  pstTmp->data.con_nums = 1077994;                                                
        else if(iCir == 195)  pstTmp->data.con_nums = 1058618;                                                
        else if(iCir == 196)  pstTmp->data.con_nums = 1046953;                                                
        else if(iCir == 197)  pstTmp->data.con_nums = 970096;                                                 
        else if(iCir == 198)  pstTmp->data.con_nums = 903487;                                                 
        else if(iCir == 199)  pstTmp->data.con_nums = 878888;                                                 
        else if(iCir == 200)  pstTmp->data.con_nums = 847808;                                                 
        else if(iCir == 201)  pstTmp->data.con_nums = 838699;                                                 
        else if(iCir == 202)  pstTmp->data.con_nums = 809124;                                                 
        else if(iCir == 203)  pstTmp->data.con_nums = 716014;                                                 
        else if(iCir == 204)  pstTmp->data.con_nums = 662486;                                                 
        else if(iCir == 205)  pstTmp->data.con_nums = 635381;                                                 
        else if(iCir == 206)  pstTmp->data.con_nums = 549519;                                                 
        else if(iCir == 207)  pstTmp->data.con_nums = 496228;                                                 
        else if(iCir == 208)  pstTmp->data.con_nums = 379290;                                                 
        else if(iCir == 209)  pstTmp->data.con_nums = 369620;                                                 
        else if(iCir == 210)  pstTmp->data.con_nums = 360567;                                                 
        else if(iCir == 211)  pstTmp->data.con_nums = 346278;                                                 
        else if(iCir == 212)  pstTmp->data.con_nums = 301822;                                                 
        else if(iCir == 213)  pstTmp->data.con_nums = 297117;                                                 
        else if(iCir == 214)  pstTmp->data.con_nums = 279626;                                                 
        else if(iCir == 215)  pstTmp->data.con_nums = 256927;                                                 
        else if(iCir == 216)  pstTmp->data.con_nums = 229569;                                                 
        else if(iCir == 217)  pstTmp->data.con_nums = 210474;                                                 
        else if(iCir == 218)  pstTmp->data.con_nums = 194215;                                                 
        else if(iCir == 219)  pstTmp->data.con_nums = 193566;                                                 
        else if(iCir == 220)  pstTmp->data.con_nums = 174065;                                                 
        else if(iCir == 221)  pstTmp->data.con_nums = 166066;                                                 
        else if(iCir == 222)  pstTmp->data.con_nums = 144390;                                                 
        else if(iCir == 223)  pstTmp->data.con_nums = 132999;                                                 
        else if(iCir == 224)  pstTmp->data.con_nums = 116745;                                                 
        else if(iCir == 225)  pstTmp->data.con_nums = 107413;                                                 
        else if(iCir == 226)  pstTmp->data.con_nums = 102916;                                                 
        else if(iCir == 227)  pstTmp->data.con_nums = 96834;                                                  
        else if(iCir == 228)  pstTmp->data.con_nums = 78474;                                                  
        else if(iCir == 229)  pstTmp->data.con_nums = 77805;                                                  
        else if(iCir == 230)  pstTmp->data.con_nums = 71179;                                                  
        else if(iCir == 231)  pstTmp->data.con_nums = 60094;                                                  
        else if(iCir == 232)  pstTmp->data.con_nums = 59012;                                                  
        else if(iCir == 233)  pstTmp->data.con_nums = 56760;                                                  
        else if(iCir == 234)  pstTmp->data.con_nums = 55431;                                                  
        else if(iCir == 235)  pstTmp->data.con_nums = 54729;                                                  
        else if(iCir == 236)  pstTmp->data.con_nums = 52448;                                                  
        else if(iCir == 237)  pstTmp->data.con_nums = 52137;                                                  
        else if(iCir == 238)  pstTmp->data.con_nums = 50702;                                                  
        else if(iCir == 239)  pstTmp->data.con_nums = 48961;                                                  
        else if(iCir == 240)  pstTmp->data.con_nums = 48852;                                                  
        else if(iCir == 241)  pstTmp->data.con_nums = 48574;                                                  
        else if(iCir == 242)  pstTmp->data.con_nums = 45232;                                                  
        else if(iCir == 243)  pstTmp->data.con_nums = 43962;                                                  
        else if(iCir == 244)  pstTmp->data.con_nums = 42268;                                                  
        else if(iCir == 245)  pstTmp->data.con_nums = 35479;                                                  
        else if(iCir == 246)  pstTmp->data.con_nums = 31120;                                                  
        else if(iCir == 247)  pstTmp->data.con_nums = 29730;                                                  
        else if(iCir == 248)  pstTmp->data.con_nums = 28624;                                                  
        else if(iCir == 249)  pstTmp->data.con_nums = 26439;                                                  
        else if(iCir == 250)  pstTmp->data.con_nums = 24247;                                                  
        else if(iCir == 251)  pstTmp->data.con_nums = 23902;                                                  
        else if(iCir == 252)  pstTmp->data.con_nums = 23159;                                                  
        else if(iCir == 253)  pstTmp->data.con_nums = 23091;                                                  
        else if(iCir == 254)  pstTmp->data.con_nums = 22281;                                                  
        else if(iCir == 255)  pstTmp->data.con_nums = 22253;                                                  
        else if(iCir == 256)  pstTmp->data.con_nums = 20205;                                                  
        else if(iCir == 257)  pstTmp->data.con_nums = 20146;                                                  
        else if(iCir == 258)  pstTmp->data.con_nums = 18518;                                                  
        else if(iCir == 259)  pstTmp->data.con_nums = 18346;                                                  
        else if(iCir == 260)  pstTmp->data.con_nums = 16015;                                                  
        else if(iCir == 261)  pstTmp->data.con_nums = 14818;                                                  
        else if(iCir == 262)  pstTmp->data.con_nums = 14103;                                                  
        else if(iCir == 263)  pstTmp->data.con_nums = 9470;                                                   
        else if(iCir == 264)  pstTmp->data.con_nums = 8246;                                                   
        else if(iCir == 265)  pstTmp->data.con_nums = 8103;                                                   
        else if(iCir == 266)  pstTmp->data.con_nums = 7642;                                                   
        else if(iCir == 267)  pstTmp->data.con_nums = 6498;                                                   
        else if(iCir == 268)  pstTmp->data.con_nums = 5522;                                                   
        else if(iCir == 269)  pstTmp->data.con_nums = 4390;                                                   
        else if(iCir == 270)  pstTmp->data.con_nums = 4014;                                                   
        else if(iCir == 271)  pstTmp->data.con_nums = 3792;                                                   
        else if(iCir == 272)  pstTmp->data.con_nums = 3154;                                                   
        else if(iCir == 273)  pstTmp->data.con_nums = 2123;                                                   
        else if(iCir == 274)  pstTmp->data.con_nums = 1520;                                                   
        else if(iCir == 275)  pstTmp->data.con_nums = 1519;                                                   
        else if(iCir == 276)  pstTmp->data.con_nums = 1352;                                                   
        else if(iCir == 277)  pstTmp->data.con_nums = 1219;                                                   
        else if(iCir == 278)  pstTmp->data.con_nums = 1198;                                                   
        else if(iCir == 279)  pstTmp->data.con_nums = 1169;                                                   
        else if(iCir == 280)  pstTmp->data.con_nums = 1108;                                                   
        else if(iCir == 281)  pstTmp->data.con_nums = 1002;                                                   
        else if(iCir == 282)  pstTmp->data.con_nums = 832;                                                    
        else if(iCir == 283)  pstTmp->data.con_nums = 766;                                                    
        else if(iCir == 284)  pstTmp->data.con_nums = 632;                                                    
        else if(iCir == 285)  pstTmp->data.con_nums = 348;                                                    
        else if(iCir == 286)  pstTmp->data.con_nums = 345;                                                    
        else if(iCir == 287)  pstTmp->data.con_nums = 200;                                                    
        else if(iCir == 288)  pstTmp->data.con_nums = 168;                                                    
        else if(iCir == 289)  pstTmp->data.con_nums = 160;                                                    
        else if(iCir == 290)  pstTmp->data.con_nums = 88;                                                     
        else if(iCir == 291)  pstTmp->data.con_nums = 0;                                                     
#endif
        list_add_tail(&(pstTmp->list), &(test_list_head.list));
    } 

 
    g_data_sort_condition = DATA_SORT_CONDITION_CON_NUM;

    return iRet;
}

// 打印链表 链表的data元素是可打印的整形    
int showDuList()
{
    int iRet = 0;

    sorted_node_t *stream_tmp;
    struct list_head *stream_pos;
    list_for_each(stream_pos, &test_list_head.list)
    {
        stream_tmp = list_entry(stream_pos, sorted_node_t, list);
        marbit_send_log(INFO,"%u \n", stream_tmp->data.con_nums);
    }

    return iRet;
}

int destroyList()
{
    int iRet = 0;
    
   sorted_node_t *stream_tmp;
   struct list_head *stream_head = &test_list_head.list;
   struct list_head *stream_pos = stream_head->next;
   struct list_head *stream_pos_next = NULL;
   
   //list_for_each(stream_pos, &stream_list_head.list) 
   while(stream_pos != stream_head)
   { 
        stream_pos_next = stream_pos->next;
        
        stream_tmp = list_entry(stream_pos, sorted_node_t, list); 
        
        list_del(stream_pos); 
        free(stream_tmp); 
        
        stream_pos =  stream_pos_next; 
   } 

    return iRet;
}
void test_quick_sort()
{
    initDuList();   //初始化
    
    marbit_send_log(INFO,"Before sorting:\n");
    showDuList();   //打印
    marbit_send_log(INFO,"\n");
    
    struct list_head *head = &test_list_head.list;
    struct list_head *first = head->next;
    struct list_head *last = head->prev;

    sorted_node_t *pstHead = list_entry(head, sorted_node_t, list); 

    struct timeval start, end;
    gettimeofday( &start, NULL );
    
    quick_sort(head, first, last);//快排序

    gettimeofday( &end, NULL );
    
    marbit_send_log(INFO,"After sorting:\n");
    showDuList();
    marbit_send_log(INFO,"\n");

    int timeuse = 1000000 * ( end.tv_sec - start.tv_sec ) + end.tv_usec - start.tv_usec;
    marbit_send_log(INFO,"timeuse = %lu\n", timeuse);
    
    destroyList();
}
void data_test()
{
    g_data_agg_class = DATA_AGG_CLASS_APP;
    g_sorted_list_index = SORTED_BASE_APP;
    g_data_sort_condition = DATA_SORT_CONDITION_CON_NUM;

    g_baselink_ip = G_BASELINK_IP_DEFAULT;
    g_display_req_line = G_DISPLAY_LINE_DEFAULT;
    g_flush_interval = G_FLUSH_INTERVAL_DEFAULT;
    g_data_agg_class_condition_appid = G_DATA_AGG_APPID_DEFAULT;

    test_get_stream_data(); 

    print_stream_list(0);

    print_sorted_list(g_sorted_list_index);

#if 0
    int sorted_condition = 0;
    printf("please input sorted condition:\n");
    printf("100: exit\n");
    printf("1: connect nums for appid\n");
    printf("2: up bps for appid\n");
    printf("3: down bps for appid\n");
    printf("4: total bits for appid\n");
    scanf("%d",&sorted_condition);
    while(100 != sorted_condition)
    {
        switch(sorted_condition)
        {
            case 1:
                  {
                        printf("sorted according to connect nums\n");
			    g_data_sort_condition = DATA_SORT_CONDITION_CON_NUM;
    			    g_data_agg_class = DATA_AGG_CLASS_APP;
                    
                        break;
                  }
            case 2:
                  {
                        printf("sorted according to up bps\n");
			    g_data_sort_condition = DATA_SORT_CONDITION_UP_BPS;
    			    g_data_agg_class = DATA_AGG_CLASS_APP;
                        break;
                  }
           case 3:
                  {
                        printf("sorted according to down bps\n");
			    g_data_sort_condition = DATA_SORT_CONDITION_DOWN_BPS;
    			    g_data_agg_class = DATA_AGG_CLASS_APP;
                        break;
                  }
           case 4:
                  {
                        printf("sorted according to total bits\n");
			    g_data_sort_condition = DATA_SORT_CONDITION_TOTAL_BITS;
    			    g_data_agg_class = DATA_AGG_CLASS_APP;
                        break;
                  }
           default: 
                    break;
        }

       // quickSort(g_sorted_list_index, g_data_sort_condition);
        print_sorted_list(g_sorted_list_index, 0);
                        
        printf("please input sorted condition:\n");
        scanf("%d",&sorted_condition);
    }
    exit(1);
#endif


#if 0
    if(DATA_AGG_CLASS_APP == g_data_agg_class)
    {
        //quickSort(g_sorted_list_index, g_data_sort_condition);
        printf("begin to start quick sort according to DATA_SORT_CONDITION_CON_NUM\n");
        g_data_sort_condition = DATA_SORT_CONDITION_CON_NUM;
        //quickSort(g_sorted_list_index, g_data_sort_condition);
        print_sorted_list(g_sorted_list_index, 0);

        printf("begin to start quick sort according to DATA_SORT_CONDITION_UP_BPS\n");
        g_data_sort_condition = DATA_SORT_CONDITION_UP_BPS;
        quickSort(g_sorted_list_index, g_data_sort_condition);
        print_sorted_list(g_sorted_list_index,0);

        printf("begin to start quick sort according to DATA_SORT_CONDITION_DOWN_BPS\n");
        g_data_sort_condition = DATA_SORT_CONDITION_DOWN_BPS;
        quickSort(g_sorted_list_index, g_data_sort_condition);
        print_sorted_list(g_sorted_list_index,0);

        printf("begin to start quick sort according to DATA_SORT_CONDITION_TOTAL_BITS\n");
        g_data_sort_condition = DATA_SORT_CONDITION_TOTAL_BITS;
        quickSort(g_sorted_list_index, g_data_sort_condition);
        print_sorted_list(g_sorted_list_index,0);
    }
 #endif

    return;
}


int test_get_stream_data()
{
    //printf("g_baselink_ip = %d\n", g_baselink_ip);
    struct msg_st mesg;

    //stream_node_t *stream_node_list = g_stream_node_list_head;
    struct sorted_node_t *sorted_node;
            
    //insert data
    int i = 0;
    for(i=1; i < 11; i++)
    {
        mesg.ip_src = mesg.port_src = mesg.ip_dst = mesg.port_dst = i;
        if(i%2 == 0)
        {
            mesg.proto = IPPROTO_TCP;
        }
        else
        {
            mesg.proto = IPPROTO_UDP;
        }
        //mesg.up_bytes = mesg.down_bytes = mesg.duration_time = i+1;
        switch(i)
        {
            case 1: 
                {
                    mesg.proto_mark = 285278208; mesg.finish_flag = 0; break;
                }
            case 2: 
                {
                    mesg.proto_mark = 285343744; mesg.finish_flag = 0; break;
                }
            case 3: 
                {
                    mesg.proto_mark = 285409280; mesg.finish_flag = 0; break;
                }
            case 4: 
                {
                    mesg.proto_mark = 285474816; mesg.finish_flag = 0; break;
                }
            case 5: 
                {
                    mesg.proto_mark = 285540352; mesg.finish_flag = 0; break;
                }
            case 6: 
                {
                    mesg.proto_mark = 285605888; mesg.finish_flag = 0; break;
                }
            case 7: 
                {
                    mesg.proto_mark = 285671424; mesg.finish_flag = 0; break;
                }
            case 8: 
                {
                    mesg.proto_mark = 285736960; mesg.finish_flag = 0; break;
                }
            case 9:
                {
                    mesg.proto_mark = 285802496; mesg.finish_flag = 0; break;
                }
            case 10: 
                {
                        mesg.proto_mark = 285868032; mesg.finish_flag = 0;break;
                }
            default: break;
        }
	 mesg.up_bytes = mesg.proto_mark % 9;
	 mesg.down_bytes = mesg.proto_mark % 5;
	 mesg.duration_time = 1;
        

        //insert into stream_list
        insert_stream_list(&mesg, &(stream_list_head.list));

        //base link just do sorted_list directly
        if(SORTED_BASE_LINK == g_sorted_list_index)
        {      
            sorted_node_t *tmp = NULL;

            uint32_t data_link_ip = 0;
            if(DATA_AGG_CLASS_IP_INNER == g_data_agg_class)
            {
                data_link_ip = mesg.ip_src;
            }
            else //if (DATA_AGG_CLASS_IP_OUTER == g_data_agg_class)
            {
                data_link_ip = mesg.ip_dst;
            }
            if(data_link_ip == g_baselink_ip)
            {
                tmp = (sorted_node_t *)malloc(sizeof(sorted_node_t));
                if(NULL != tmp)
                {
                    bzero(tmp, sizeof(sorted_node_t));
                    
                    tmp->data.link_app = mesg.proto_mark;
                    tmp->data.link_proto = mesg.proto;

                    sprintf( tmp->data.link_con_info, "%u.%u.%u.%u:%u--%u.%u.%u.%u:%u", 
                              IPQUADS(mesg.ip_src), ntohs(mesg.port_src), IPQUADS(mesg.ip_dst),ntohs(mesg.port_dst));
                    tmp->data.link_duration_time = mesg.duration_time;
                    tmp->data.link_up_size = mesg.up_bytes * 8;
                    tmp->data.link_down_size = mesg.down_bytes * 8;

                     //fill sorted_list
                    do_process_insert_sorted_list(tmp, g_data_agg_class, g_sorted_list_index, g_data_sort_condition);
                }
            }
        }
        //others should be first do aggregation then do sorted_list
        else
        {
            //fill in app_statistics
            process_data_aggregation_list(&mesg,
                                                        g_data_agg_class, 
                                                        g_sorted_list_index, 
                                                        g_data_sort_condition);
         }
    }

  #if 1
      //repeat data
    i = 0;
    for(i=1; i < 11; i++)
    {
        //print_sorted_list(g_sorted_list_index, 0);
        
        mesg.ip_src = mesg.port_src = mesg.ip_dst = mesg.port_dst  = i;
        if(i%2 == 0)
        {
            mesg.proto = IPPROTO_TCP;
        }
        else
        {
            mesg.proto = IPPROTO_UDP;
        }
        mesg.up_bytes = mesg.down_bytes = mesg.duration_time = i+5;
        switch(i)
        {
            case 1: 
                {
                    mesg.proto_mark = 285278208; mesg.finish_flag = 0; break;
                }
            case 2: 
                {
                    mesg.proto_mark = 285343744; mesg.finish_flag = 0; break;
                }
            case 3: 
                {
                    mesg.proto_mark = 285409280; mesg.finish_flag = 0; break;
                }
            case 4: 
                {
                    mesg.proto_mark = 285474816; mesg.finish_flag = 0; break;
                }
            case 5: 
                {
                    mesg.proto_mark = 285540352; mesg.finish_flag = 0; break;
                }
            case 6: 
                {
                    mesg.proto_mark = 285605888; mesg.finish_flag = 0; break;
                }
            case 7: 
                {
                    mesg.proto_mark = 285671424; mesg.finish_flag = 0; break;
                }
            case 8: 
                {
                    mesg.proto_mark = 285736960; mesg.finish_flag = 0; break;
                }
            case 9:
                {
                    mesg.proto_mark = 285802496; mesg.finish_flag = 0; break;
                }
            case 10: 
                {
                        mesg.proto_mark = 285868032; mesg.finish_flag = 1;break;
                }
            default: break;
        }

        //insert into stream_list
        insert_stream_list(&mesg, &(stream_list_head.list));
        
        //base link just do sorted_list directly
        if(SORTED_BASE_LINK == g_sorted_list_index)
        {      
            sorted_node_t *tmp = NULL;

            uint32_t data_link_ip = 0;
            if(DATA_AGG_CLASS_IP_INNER == g_data_agg_class)
            {
                data_link_ip = mesg.ip_src;
            }
            else //if (DATA_AGG_CLASS_IP_OUTER == g_data_agg_class)
            {
                data_link_ip = mesg.ip_dst;
            }
            if(data_link_ip == g_baselink_ip)
            {
                tmp = (sorted_node_t *)malloc(sizeof(sorted_node_t));
                if(NULL != tmp)
                {
                    bzero(tmp, sizeof(sorted_node_t));
                    
                    tmp->data.link_app = mesg.proto_mark;
                    tmp->data.link_proto = mesg.proto;

                    sprintf( tmp->data.link_con_info, "%u.%u.%u.%u:%u--%u.%u.%u.%u:%u", 
                              IPQUADS(mesg.ip_src), ntohs(mesg.port_src), IPQUADS(mesg.ip_dst),ntohs(mesg.port_dst));
                    tmp->data.link_duration_time = mesg.duration_time;
                    tmp->data.link_up_size = mesg.up_bytes * 8;
                    tmp->data.link_down_size = mesg.down_bytes * 8;

                     //fill sorted_list
                    do_process_insert_sorted_list(tmp, 
                                                                    g_data_agg_class, 
                                                                    g_sorted_list_index, 
                                                                    g_data_sort_condition);
                }
            }
        }
        //others should be first do aggregation then do sorted_list
        else
        {
            //update in app_statistics
            process_data_aggregation_list(&mesg, 
                                                        g_data_agg_class, 
                                                        g_sorted_list_index, 
                                                        g_data_sort_condition);
        }
    }

#endif
}

