#!/usr/bin/python
"""
# Want to construct a:
#  Fragmented IPv6 UDP packet

# Based on example from:
#  https://media.blackhat.com/bh-eu-12/Atlasis/bh-eu-12-Atlasis-Attacking_IPv6-Slides.pdf

Usage:
    -s source address
    -d destination address

"""

import getopt
import sys

from scapy.all import *

srcip = None
dstip = None
dstport = 5555

def ipv6_udp_frag(srcip,dstip,dst_port):

    # ether
    eth_fvm05=Ether(dst="52:54:bb:cc:dd:05") # mac of fvm05%eth1
    eth_fvm03=Ether(dst="52:54:bb:cc:dd:03") # mac of fvm03%eth1

    #IPv6 parameters
    #sip="fee0:200::106" (now srcip)
    #dip="fee0:200::42"
    #dip="fee0:cafe::42" (now dstip)
    #dip="fee0:cafe::102"
    #conf.route6.add("::",gw="fee0:200::1")
    conf.route6.add("fee0:cafe::/64",gw="fee0:200::1")

    payload1="AAAAAAAA"
    payload2="BBBBBBBB"
    payload3="CCCCCCCC"
    payload_all="AAAAAAAABBBBBBBBCCCCCCCC"
    ipv6_1=IPv6(src=srcip, dst=dstip, plen=16)
    #icmpv6=ICMPv6EchoRequest(cksum=0x7d2b)

    # Calculate/find the correct checksum
    #  by constructing the NONE-fragmented UDP packet
    csum_udp0=UDP(sport=12345, dport=dst_port)
    csum_pkt0=ipv6_1/csum_udp0/payload_all
    #
    # Scapy hack to force chksum calculation
    del csum_pkt0.chksum
    csum_pkt0 = csum_pkt0.__class__(str(csum_pkt0))
    #csum_pkt0.show()
    csum_pkt0.show2()
    print "chksum:", hex(csum_pkt0.chksum)

    # Need to set the chksum explicitly, as it cannot be calculated
    #  correctly as we construct fragments here
    udp=UDP(sport=12345, dport=dst_port, chksum=csum_pkt0.chksum, len=32)

    #Fragment
    # http://www.tcpipguide.com/free/t_IPv6DatagramMainHeaderFormat-2.htm
    # next-header = nh = 58 = ICMPv6
    # next-header = nh = 17 = UDP
    # offset is multiplum of 8
    # "m" == More fragments to follow
    frag1=IPv6ExtHdrFragment(offset=0, m=1, id=502, nh=17)
    frag2=IPv6ExtHdrFragment(offset=1, m=1, id=502, nh=17)
    frag3=IPv6ExtHdrFragment(offset=2, m=1, id=502, nh=17)
    frag4=IPv6ExtHdrFragment(offset=3, m=0, id=502, nh=17)

    #packet1=ipv6_1/frag1/icmpv6
    packet1=ipv6_1/frag1/udp
    packet2=ipv6_1/frag2/payload1
    packet3=ipv6_1/frag3/payload2
    packet4=ipv6_1/frag4/payload3

    #send(packet1)
    #send(packet2)
    print "Sending fragmented UDP packet(s)"
    # Notice, not sending packets in right order, BUT when a hosts
    #  receives the packets, they have been reordered... hmmm?!
    sendp(eth_fvm03/packet3)
    sendp(eth_fvm03/packet2)
    sendp(eth_fvm03/packet1)
    sendp(eth_fvm03/packet4)

    #packet1.show()

    # Extra send the none frag packet as a test
    if (0):
            print "Sending the NONE-fragmented version of the UDP packet"
            csum_pkt0.sport=11111
            print "- With changed source port to:", csum_pkt0.sport
            del csum_pkt0.chksum #force recalc of chksum
            #csum_pkt0.show2()
            sendp(eth_fvm03/csum_pkt0)

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

    if not srcip or not dstip:
        usage("Must specify source (-s) and destination (-d)")

    ipv6_udp_frag(srcip,dstip,dstport)
