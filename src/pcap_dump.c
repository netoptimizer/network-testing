/* 
 Example usage of libpcap.
 Collection data with pcap and writing directly to an pcap dump file.

 Author: Jesper Dangaard Brouer <hawk@diku.dk>, d.15/10-2001.
 $Id: pcap_dump.c,v 1.3 2001/10/20 16:49:47 hawk Exp $
 */

#include<pcap.h>
#include <stdio.h>

int main() {
  pcap_t        *pcap;     /* Packet capture descriptor */
  pcap_dumper_t *pcapfile; /* Pointer to a pcap file (opened or created) */
  struct pcap_stat stat;   /* Some pcap stats */
  char errbuf[PCAP_ERRBUF_SIZE];

  char filename[] ="/tmp/dumpfile.pcap";
  int packet_count = 10000;

  char device[] = "eth0";
  int snaplen = 2000; 
  int promisc = 0;
  int timeout = 1000;  /* Timeout in milliseconds */

  extern char pcap_version[];

  printf("\nExample usage of libpcap. (version %s)\n", pcap_version);
  printf("-------------------------\n");
  printf("Collection %d packets with pcap\n", packet_count);
  printf(" and writing a pcap dumpfile: %s\n", filename);
  printf("\n");

  if ((pcap = pcap_open_live(device, snaplen, promisc, timeout, errbuf)) == NULL) {
    fprintf(stderr, "Error from pcap_open_live(): %s\n", errbuf); 
    exit(1);
  }

  if ((pcapfile = pcap_dump_open(pcap, filename)) == NULL) {
    fprintf(stderr, "Error from pcap_dump_open(): %s\n", pcap_geterr(pcap)); 
    exit(1);
  }

  if ((pcap_loop(pcap, packet_count, pcap_dump, (u_char *)pcapfile)) != 0) {
    fprintf(stderr, "Error from pcap_loop(): %s\n", pcap_geterr(pcap)); 
    exit(1);
  }

  if ((pcap_stats(pcap, &stat) != 0)) {
    fprintf(stderr, "Error from pcap_stats(): %s\n", pcap_geterr(pcap)); 
    exit(1);
  } 
  else {
    printf("Statistik from pcap\n");
    printf(" packets received  : %d\n", stat.ps_recv);
    printf(" packets dropped   : %d\n", stat.ps_drop);
    printf(" drops by interface: %d\n", stat.ps_ifdrop);
  }

  pcap_dump_close(pcapfile);
  pcap_close(pcap);    
}
