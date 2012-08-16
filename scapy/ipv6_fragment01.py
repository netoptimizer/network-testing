#!/usr/bin/python

# Based on example from:
#  https://media.blackhat.com/bh-eu-12/Atlasis/bh-eu-12-Atlasis-Attacking_IPv6-Slides.pdf

from scapy.all import *

# Want to construct a:
#  Fragmented IPv6 UDP packet

# ether
eth_fvm05=Ether(dst="52:54:bb:cc:dd:05") # mac of fvm05%eth1
eth_fvm03=Ether(dst="52:54:bb:cc:dd:03") # mac of fvm03%eth1

#IPv6 parameters
sip="fee0:200::106"
#dip="fee0:200::42"
dip="fee0:cafe::42"
#dip="fee0:cafe::102"
#conf.route6.add("::",gw="fee0:200::1")
conf.route6.add("fee0:cafe::/64",gw="fee0:200::1")

payload1="AAAAAAAA"
ipv6_1=IPv6(src=sip, dst=dip, plen=16)
#ipv6_1=IPv6(src=sip, dst=dip, plen=24)
#icmpv6=ICMPv6EchoRequest(cksum=0x7d2b) 

# Calculate/find the correct checksum
#  by constructing the NONE-fragmented UDP packet
csum_udp0=UDP(sport=12345, dport=5001)
csum_pkt0=ipv6_1/csum_udp0/payload1
#
# Scapy hack to force chksum calculation
del csum_pkt0.chksum
csum_pkt0 = csum_pkt0.__class__(str(csum_pkt0))
#csum_pkt0.show()
print "chksum:", hex(csum_pkt0.chksum)

# Need to set the chksum explicitly, as it cannot be calculated
#  correctly as we construct fragments here 
udp=UDP(sport=12345, dport=5001, chksum=csum_pkt0.chksum, len=16)

#Fragment 
# http://www.tcpipguide.com/free/t_IPv6DatagramMainHeaderFormat-2.htm
# next-header = nh = 58 = ICMPv6
# next-header = nh = 17 = UDP
frag1=IPv6ExtHdrFragment(offset=0, m=1, id=502, nh=17) 
frag2=IPv6ExtHdrFragment(offset=1, m=0, id=502, nh=17) 

#packet1=ipv6_1/frag1/icmpv6 
packet1=ipv6_1/frag1/udp
#packet1=ipv6_1/frag1/udp/payload1
packet2=ipv6_1/frag2/payload1 

#send(packet1) 
#send(packet2)
print "Sending fragmented UDP packet(s)"
sendp(eth_fvm03/packet2)
#delay(2)
sendp(eth_fvm03/packet1) 

#packet1.show()

# Extra send the none frag packet as a test
if (0):
	print "Sending the NONE-fragmented version of the UDP packet"
	csum_pkt0.sport=11111
	print "- With changed source port to:", csum_pkt0.sport
	del csum_pkt0.chksum #force recalc of chksum
	#csum_pkt0.show2()
	sendp(eth_fvm03/csum_pkt0)

