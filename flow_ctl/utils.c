#define _SVID_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <syslog.h>

#include "debug.h"
#include "list.h"
#include "init.h"

void ErrorMessage(const char *format,...)
{
    char buf[STD_BUF+1];
    va_list ap;

    va_start(ap, format);

    if(pv.daemon_flag)
    {
        vsnprintf(buf, STD_BUF, format, ap);
        syslog(LOG_CONS | LOG_DAEMON | LOG_ERR, "%s", buf);
    }
    else
    {
        vfprintf(stderr, format, ap);
    }
    va_end(ap);
}
void PrintError(char *str)
{
    if(pv.daemon_flag)
        syslog(LOG_CONS | LOG_DAEMON | LOG_ERR, "%s:%m", str);
    else
        perror(str);
}
/*
 * Function: FatalError(const char *, ...)
 *
 * Purpose: When a fatal error occurs, this function prints the error message
 *          and cleanly shuts down the program
 *
 * Arguments: format => the formatted error string to print out
 *            ... => format commands/fillers
 *
 * Returns: void function
 */
void FatalError(const char *format,...)
{
    char buf[STD_BUF+1];
    va_list ap;

    va_start(ap, format);

    vsnprintf(buf, STD_BUF, format, ap);

    if(pv.daemon_flag)
    {
        syslog(LOG_CONS | LOG_DAEMON | LOG_ERR, "FATAL ERROR: %s", buf);
    }
    else
    {
        fprintf(stderr, "ERROR: %s", buf);
        fprintf(stderr,"Fatal Error, Quitting..\n");
    }

    exit(1);
}
void FatalPrintError(char *msg)
{
    PrintError(msg);
    exit(1);
}

/****************************************************************************
 *
 * Function  : DefineIfaceVar()
 * Purpose   : Assign network address and network mast to IFACE_ADDR_VARNAME
 *             variable.
 * Arguments : interface name (string) netaddress and netmask (4 octets each)
 * Returns   : void function
 *
 ****************************************************************************/
void DefineIfaceVar(char *iname, uint8_t * network, uint8_t * netmask)
{
    char valbuf[32];
    char varbuf[BUFSIZ];

    snprintf(varbuf, BUFSIZ, "%s_ADDRESS", iname);

    snprintf(valbuf, 32, "%d.%d.%d.%d/%d.%d.%d.%d",
            network[0] & 0xff, network[1] & 0xff, network[2] & 0xff,
            network[3] & 0xff, netmask[0] & 0xff, netmask[1] & 0xff,
            netmask[2] & 0xff, netmask[3] & 0xff);

//    VarDefine(varbuf, valbuf);
}

void LogMessage(const char *format,...)
{
    char buf[STD_BUF+1];
    va_list ap;

    if(!pv.daemon_flag)
        return;

    va_start(ap, format);

    if(pv.daemon_flag)
    {
        vsnprintf(buf, STD_BUF, format, ap);
        syslog(LOG_DAEMON | LOG_NOTICE, "%s", buf);
    }
    else
    {
        vfprintf(stderr, format, ap);
    }

        vsnprintf(buf, STD_BUF, format, ap);
        syslog(LOG_DAEMON | LOG_NOTICE, "%s", buf);

    va_end(ap);
}

/****************************************************************************
 *
 * Function: copy_argv(u_char **)
 *
 * Purpose: Copies a 2D array (like argv) into a flat string.  Stolen from
 *          TCPDump.
 *
 * Arguments: argv => 2D array to flatten
 *
 * Returns: Pointer to the flat string
 *
 ****************************************************************************/
char *copy_argv(char **argv)
{
    char **p;
    uint32_t len = 0;
    char *buf;
    char *src, *dst;
    void ftlerr(char *,...);

    p = argv;
    if(*p == 0)
        return 0;

    while(*p)
        len += strlen(*p++) + 1;

    buf = (char *) malloc(len);

    if(buf == NULL)
    {
        E("malloc() failed: %s\n", strerror(errno));
        FatalError("malloc() failed: %s\n", strerror(errno));
    }
    p = argv;
    dst = buf;

    while((src = *p++) != NULL)
    {
        while((*dst++ = *src++) != '\0');
        dst[-1] = ' ';
    }

    dst[-1] = '\0';

    return buf;
}

