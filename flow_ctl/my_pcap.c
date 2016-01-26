#include "my_pcap.h"
#include "init.h"
#include <time.h>
#if 0
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
struct timeval {
    long    tv_sec;         /* seconds */
    long    tv_usec;        /* and microseconds */
};
#endif
#define PCAP_VERSION_MAJOR 2
#define PCAP_VERSION_MINOR   4   
#define PCAP_ERRBUF_SIZE   256
#define PCAP_IF_LOOPBACK   0x00000001
#define MODE_CAPT   0
#define MODE_STAT   1
int create_pcap_file()
{
    pv.pcap_fp = fopen("test.pcap", "wb");
}
static inline void
calculate_timestamp(struct timeval *ts) {
    uint64_t cycles;
    struct timeval cur_time;

    cycles = rte_get_timer_cycles() - start_cycles;
    cur_time.tv_sec = cycles / hz;
    cur_time.tv_usec = (cycles % hz) * 10e6 / hz;
    timeradd(&start_time, &cur_time, ts);
}

int pcap_write_head(FILE *fp, int linktype, int thiszone, int snaplen)  
{  
    struct pcap_file_header hdr;     //声明一个pcap_file_header 对象

    hdr.magic = TCPDUMP_MAGIC;  
    hdr.version_major = PCAP_VERSION_MAJOR;  
    hdr.version_minor = PCAP_VERSION_MINOR;   //固定填充

    hdr.thiszone = thiszone;  
    hdr.snaplen = snaplen;  
    hdr.sigfigs = 0;  
    hdr.linktype = linktype;  

    if (fwrite((char *)&hdr, sizeof(hdr), 1, fp) != 1)  
        return 0;
#if 0
    struct pcap_pkthdr h; 
    calculate_timestamp(&h.ts);
    h.caplen = 4096;
    h.len = len;
#endif
    fseek(fp, sizeof(struct pcap_pkthdr), SEEK_CUR);         
    return 1;  
}

int pcap_fill_data(FILE *fp )
{
    if (1 != fwrite(buf, strlen(buf), 1, fp))
    {
        printf("write data err\n");
        return (-1);
    }
}

