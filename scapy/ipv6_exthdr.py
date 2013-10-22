#!/usr/bin/python
"""
Want to construct a IPv6 packet with some exthdr
- This is for testing how the kernel handles skipping exthdrs

Found some inspiration here:
  http://www.packetlevel.ch/html/scapy/scapyipv6.html

 Usage:
    -s source address
    -d destination address
    -p destination port

"""

import getopt
import sys

from scapy.all import *

srcip = "fee0:200::106"
dstip = "fee0:cafe::42"
dstport = 5555

conf.route6.add("fee0:cafe::/64",gw="fee0:200::1")

def ipv6_exthdr_udp(srcip,dstip,dst_port):
    
    payload1="AAAAAAAA"

    eth_fvm03=Ether(dst="52:54:bb:cc:dd:03") # mac of fvm03%eth1
    dst_mac=eth_fvm03

    ipv6=IPv6(src=srcip, dst=dstip)
    #ipv6=IPv6(dst=dstip)

    udp=UDP(sport=12345, dport=dst_port)

    ### Try Router exthdr
    #exthdr_route=IPv6ExtHdrRouting()
    #exthdr_route.addresses=["2001:db8:dead::1","2001:db8:dead::1"]
    #exthdr_route.addresses=["fee0:cafe::42"]
    #packet=(ipv6/exthdr_route/udp/payload1)

    ### Create Hop by Hop header
    #exthdr_hbh=IPv6ExtHdrHopByHop()
    #exthdr_hbh.nh=17
    #exthdr_hbh.options=[PadN(otype=0x01,optlen=0x08,optdata=("0"*8))]
    #packet=(ipv6/exthdr_hbh/udp/payload1)

    ### Try Destination Options extension header
    exthdr_destopt=IPv6ExtHdrDestOpt()
    #packet=(ipv6/exthdr_destopt/udp/payload1)
    packet=(ipv6/exthdr_destopt/exthdr_destopt/udp/payload1)

    packet.show2()

    sendp(dst_mac/packet)


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
            elif o == '-p': dstport = int(a)
            else: raise Warning, 'EDOOFUS - Programming error'
    except getopt.GetoptError, e:
        usage(e)

    if not srcip or not dstip:
        usage("Must specify source (-s) and destination (-d)")

    ipv6_exthdr_udp(srcip,dstip,dstport)
