/* 
 Example usage of libpcap.
 Reading data with pcap and printing timestamp + length.

 Author: Jesper Dangaard Brouer <hawk@diku.dk>, d.23/1-2002.
 $Id: pcap_read.c,v 1.1 2002/01/23 13:25:38 hawk Exp $
 */

#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>

/* Prints packet timestamp and packet lenght */
void pkt_handler(u_char *tmp, struct pcap_pkthdr *hdr, u_char *pkt_data) {
  printf("%ld:%ld (%ld)\n", hdr->ts.tv_sec, hdr->ts.tv_usec, hdr->len);
}

int main(int argc, char* argv[]) {
  pcap_t        *pcap;     /* Packet capture descriptor */
  struct pcap_stat stat;   /* Some pcap stats */
  char errbuf[PCAP_ERRBUF_SIZE];

  char *filename = argv[1]; /* Quick and dirty */
  if (!filename) {
    fprintf(stderr, "ERROR: Need dumpfile input as argv[1]\n");
    exit(1);
  }
  int packet_count = 10;

  extern char pcap_version[];

  printf("\nExample usage of libpcap. (version %s)\n", pcap_version);
  printf("-------------------------\n");
  printf("READING %d packets with pcap\n", packet_count);
  printf(" from a pcap dumpfile: %s\n", filename);
  printf("\n");

  if ((pcap = pcap_open_offline(filename, errbuf)) == NULL) {
    fprintf(stderr, "Error from pcap_open_offline(): %s\n", errbuf); 
    exit(1);
  }

  if ((pcap_loop(pcap, packet_count, (void*)pkt_handler, NULL)) != 0) {
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

  pcap_close(pcap);
  return 0;
}
