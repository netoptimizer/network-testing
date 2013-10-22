
Links:
------

http://www.secdev.org/projects/scapy/doc/usage.html
http://theitgeekchronicles.files.wordpress.com/2012/05/scapyguide1.pdf


Using data from a PCAP file
===========================

Read the pcap::

 pkts=rdpcap("single-inval-tcp-packet.cap")
 print "Read" , len(pkts), "packet(s)"

Print first packet::

 >>> pkts[0]
 <Ether  dst=00:25:90:c0:bf:00 src=00:0d:66:fe:6c:00 type=IPv4 |<IP  version=4L ihl=5L tos=0x0 len=48 id=53849 flags=DF frag=0L ttl=117 proto=tcp chksum=0x660a src=205.59.5.238 dst=146.28.204.30 options=[] |<TCP  sport=ftranhc dport=capioverlan seq=4278418718 ack=0 dataofs=7L reserved=0L flags=S window=16384 chksum=0xf0ca urgptr=0 options=[('MSS', 1460), ('NOP', None), ('MSS', '\x03')] |>>>

Print detailed version::

 >>> pkts[0].show()
 ###[ Ethernet ]###
  dst= 00:25:90:c0:bf:00
  src= 00:0d:66:fe:6c:00
  type= IPv4
 ###[ IP ]###
     version= 4L
     ihl= 5L
     tos= 0x0
     len= 48
     id= 53849
     flags= DF
     frag= 0L
     ttl= 117
     proto= tcp
     chksum= 0x660a
     src= 205.59.5.238
     dst= 146.28.204.30
     \options\
 ###[ TCP ]###
        sport= ftranhc
        dport= capioverlan
        seq= 4278418718
        ack= 0
        dataofs= 7L
        reserved= 0L
        flags= S
        window= 16384
        chksum= 0xf0ca
        urgptr= 0
        options= [('MSS', 1460), ('NOP', None), ('MSS', '\x03')]

Sending the packet::

 sendp(pkts[0])


Modifying parts of a packet
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Get refererence to the first packet from the pcap::

 pkt = pkts[0]

We only want the IP layer::

 ip = pkt.getlayer(IP)

Now we want to change the src and dst IP ::

 ip.src = "10.10.10.10"
 ip.dst = "192.168.11.103"

Problem the IP "chksum" have not been updated, to fix this either set
ip.chksum=None or "del" the field ::

 del ip.chksum
 ip.show2()

If you need to extract, and not let is be generated on send() time, do
the following python hack::

 # Scapy hack to force chksum calculation
 ip = ip.__class__(str(ip))
 print "ip.chksum:", hex(ip.chksum)

Change the TCP dest port::

 ip.getlayer(TCP).dport=80

Send the IP part with::

 send(ip)

Send the full packet with Ethernet headers::

 sendp(pkt)

Writing the modified packet back to a pcap file::

 >>> wrpcap("/tmp/scapy01.pcap", p);
 >>> wrpcap("/tmp/scapy02.pcap", ip);


Construct
=========

ip1=IP(dst="192.168.11.103")
tcp1=TCP(dport=80)

send(ip1/tcp1)

tcp1.options = [('Timestamp', (10057422L, 127927993L))]


# Reproducer for SYNPROXY crash
ip1=IP(dst="192.168.11.103")
tcp1=TCP(dport=80)
tcp1.options= [('MSS', 31354), ('NOP', None), ('EOL', None)] 
tcp1.dataofs = 8 # <-- causes the crash
send(ip1/tcp1)
