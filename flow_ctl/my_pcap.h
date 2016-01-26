#ifndef __MY_PCAP_H
#define __MY_PCAP_H
#include <stdint.h>
struct pcap_file_header
{
    uint32_t magic;
    uint16_t version_major;
    uint16_t version_minor;
    uint32_t thiszone; /* gmt to local correction */
    uint32_t sigfigs;    /* accuracy of timestamps */
    uint32_t snaplen;    /* max length saved portion of each pkt */
    uint32_t linktype;   /* data link type (LINKTYPE_*) */
};

struct pcap_pkthdr {
    struct timeval ts;  /* time stamp */
    uint32_t caplen; /* length of portion present */
    uint32_t len;    /* length this packet (off wire) */
};
#if 0
struct timeval {
    long    tv_sec;         /* seconds */
    long    tv_usec;        /* and microseconds */
};
#endif
#endif
