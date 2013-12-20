/*
 Quick pcap program that shows the number of packets within X ms.

 Author: Jesper Dangaard Brouer <hawk@comx.dk>.
*/

#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>

#define RESOLUTION_DEFAULT 300; /* one ms is 1000 */
static int resolution = RESOLUTION_DEFAULT;

/* Prints packet timestamp and packet lenght */
void pkt_handler(u_char *tmp, struct pcap_pkthdr *hdr, u_char *pkt_data) {
  static int cnt;
  static int size;
  static struct timeval prev; /* time stamp */
  static unsigned int thresh;

  cnt++;
  size+=hdr->len;
  //  if (prev.tv_sec == hdr->ts.tv_sec) {
  if (thresh == (hdr->ts.tv_usec / resolution)) {
    printf("+");
  } else {
    printf("cnt[%d] sz:[%d]\n %ld:%ld\t(len:%ld) ",
	   cnt, size, hdr->ts.tv_sec, hdr->ts.tv_usec, hdr->len);
    cnt=0;
    size=0;
  }

  prev.tv_sec  = hdr->ts.tv_sec;
  prev.tv_usec = hdr->ts.tv_usec;
  thresh = hdr->ts.tv_usec / resolution;
}

int main(int argc, char* argv[]) {
  pcap_t          *pcap;   /* Packet capture descriptor */
  struct pcap_stat stat;   /* Some pcap stats */
  char errbuf[PCAP_ERRBUF_SIZE];

  char *filename = argv[1]; /* Quick and dirty */
  if (!filename) {
    fprintf(stderr, "ERROR: Need dumpfile input as argv[1]\n");
    exit(1);
  }
  int packet_count = 10000;

  if (argv[2]) {
    resolution = atoi(argv[2]);
    if (!resolution)
      resolution = RESOLUTION_DEFAULT;
  }

  extern char pcap_version[];

  printf("\nNumber of packets within X ms (libpcap version %s)\n", pcap_version);
  printf("-------------------------\n");
  printf("READING %d packets with pcap\n", packet_count);
  printf(" from a pcap dumpfile: %s\n", filename);
  printf("Resolution: %3.3f ms\n", (float)resolution / 1000);

  printf("\n");

  if ((pcap = pcap_open_offline(filename, errbuf)) == NULL) {
    fprintf(stderr, "Error from pcap_open_offline(): %s\n", errbuf);
    exit(1);
  }

  if ((pcap_loop(pcap, packet_count, (void*)pkt_handler, NULL)) != 0) {
    fprintf(stderr, "Error from pcap_loop(): %s\n", pcap_geterr(pcap));
    exit(1);
  }
  printf("\n");

  printf("Packes within resolution: %1.3f ms\n", (float)resolution / 1000);

  pcap_close(pcap);
  return 0;
}
