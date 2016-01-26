#ifndef __EXPORT_H
#define __EXPORT_H

enum EXPORT_FILE_TYPE {
    APP_CLASS_FILE_TYPE = 0,
    PATTERN_FILE_TYPE,
    PLACE_FILE_TYPE,
    RULE_FILE_TYPE,
    DPI_SHOW_FILE_TYPE,
    STATICTICS_FILE_TYPE,
    STATICTICS_RULE_FLOW,
    MAX_EXPORT_FILE_TYPE,
};

int export_file(const enum EXPORT_FILE_TYPE type, const char *input_buf, int flag);
int init_export_file();
#endif
