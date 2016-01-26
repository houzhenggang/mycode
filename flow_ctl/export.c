#include <rte_ring.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <stdio.h>
#include <string.h>
#include "export.h"
#include "init.h"
#include "debug.h"

struct export_file_struct {
    FILE *fp;
    const char *name;
};

static struct export_file_struct export_file_array[] = 
{
    {NULL, "/dev/shm/sft_dpi/app_class.sft"},
    {NULL, "/dev/shm/sft_dpi/pattern.sft"},
    {NULL, "/dev/shm/sft_dpi/place.sft"},
    {NULL, "/dev/shm/sft_dpi/rule.sft"},
    {NULL, "/dev/shm/sft_dpi/dpi_show.sft"},
    {NULL, "/dev/shm/sft_dpi/statictics.sft"},
    {NULL, "/dev/shm/sft_dpi/rule_flow_static.sft"},
};


int export_file(const enum EXPORT_FILE_TYPE type, const char *input_buf, int flag)
{

    
    if (flag == 0) {
        export_file_array[(int)type].fp = fopen(export_file_array[(int)type].name, "w+");
        if (unlikely(export_file_array[(int)type].fp == NULL)) {
            E("not [%s] file\n",export_file_array[(int)type].name);
            perror("fopen()");
            return -1;
        }   
     } else if (flag == 1) {
        fwrite(input_buf,1, strlen(input_buf),export_file_array[(int)type].fp); 
    } else if (flag == 3) {
        export_file_array[(int)type].fp = fopen(export_file_array[(int)type].name, "w+");
        if (unlikely(export_file_array[(int)type].fp == NULL)) {
            E("fopen");
            perror("fopen()");
            return -1;
        }   

        fwrite(input_buf,1, strlen(input_buf),export_file_array[(int)type].fp); 
        fclose(export_file_array[(int)type].fp);
    } else {
        if (export_file_array[(int)type].fp)
            fclose(export_file_array[(int)type].fp);
        //export_file_array[(int)type].fp = NULL;
    }
    return 0;
}


int init_export_file()
{
    int i;
    char file[256] = {0};

#if 0    
   for (i = 0; i < MAX_EXPORT_FILE_TYPE; i++) {
        bzero(file, 256);
        strncpy(file, pv.sft_dpi_path, strlen(pv.sft_dpi_path));
        strncat(file, export_file_array[i].name, strlen(export_file_array[i].name));
        export_file_array[i].fp = fopen(file, "w+");
        if (unlikely(export_file_array[i].fp == NULL)) {
            perror("fopen()");
            return -1;
        } 
    }
#endif
}
