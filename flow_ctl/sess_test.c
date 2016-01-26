#include <arpa/inet.h>

#include <rte_ether.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#include <unistd.h>
#include "session_mgr.h"

#define NIPQUAD(addr) \
    ((unsigned char *)&addr)[0], \
    ((unsigned char *)&addr)[1], \
    ((unsigned char *)&addr)[2], \
    ((unsigned char *)&addr)[3]

int main(int argc, char *argv[])
{
    int hash_len = 0;
    int hash_size = 48;
    struct sess_hash *hash = NULL;
    struct sess_entry * p = NULL;
    int i = 0;

    if(argc != 3)
    {
        printf("error usage: %s:hash_size test_len\n", argv[0]);
        return -1;
    }

    hash_size = atoi(argv[1]);
    hash_len = atoi(argv[2]);

    if((hash = hash_init(hash_size)) == NULL)
    {
        printf("hash_init failed. \n");
        return 1;
    }

    struct key_5tuple key;

    for(i= 0; i < hash_len; i++)
    {
        key.ip_src = 0xc0a80201 + i;
        key.ip_dst = 0xdb8e4e01 + i * 2;
        key.port_src = 10000 + i * 4;
        key.port_dst = 10000 + i * 8;
        key.proto = 7;
        p = hash_find_and_new(hash, &key, ipaddr_hash_func);
        if(p)
        {
            p->pack_num = i;
            p->pack_bytes = i * 1024;
        }
        //sleep(1);
    }

    //sleep(10);

    for(i = 0; i < hash_len; i++)
    {
        key.ip_src = 0xc0a80201 + i;
        key.ip_dst = 0xdb8e4e01 + i * 2;
        key.port_src = 10000 + i * 4;
        key.port_dst = 10000 + i * 8;
        key.proto = 7;
        p = hash_find(hash, &key, ipaddr_hash_func);
        if(p)
        {
            uint32_t src = ntohl(p->ip_src);
            uint32_t dst = ntohl(p->ip_dst);
            printf("entry:%p-%u.%u.%u.%u:%u---%u.%u.%u.%u:%u[%u]-[%u|%u]\n", p, NIPQUAD(src), p->port_src, 
                    NIPQUAD(dst), p->port_dst, p->proto, p->pack_num, p->pack_bytes);
        }
    }

    printf("hash: size:%lu, all count:%lu, curr:%lu, rnd:%d\n", hash->max_hash_size, 
           hash->max_entry_size, hash->curr_entry_count, hash->sess_hash_rnd);

    return 0;
}
