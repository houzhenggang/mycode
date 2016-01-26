#ifndef     __DPI_LOGIN_ANALYSIS_H
#define     __DPI_LOGIN_ANALYSIS_H
#include "pattern_tbl.h"
void login_parse(void *data, pattern_t *pattern, size_t offset, uint8_t rule_status); //rule_status:rulse 规则跑完为1，中间状态为0;
#endif 
