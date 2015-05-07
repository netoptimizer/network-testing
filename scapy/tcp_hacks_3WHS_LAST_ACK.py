#!/usr/bin/python
"""

 TCP hacks for network testing
  - Cause servers socket to enter LAST-ACK state
  - Reproducer for conntrack bug regarding RFC5961 challenge ACK

Pre-setup:

 Use iptables to suppress the kernel from sending a RST packet, when
 receiving the SYN-ACK packet response (to our fake SYN).  As the
 kernel have not initiated the connection, it cannot associate the
 SYN-ACK with any (listen) socket.  Thus, it will send back a RST
 indicating there is no-one listening on that port.  (It might also
 send a ICMP port-unreachable.)

 (Note: Traffic to and from Scapy will not be filtered by iptables)

 Shell setup:
  export SRCIP=192.168.1.5
  export DSTIP=192.168.1.42
  export DPORT=6666

 Run these commands as root:

  iptables -A OUTPUT -p tcp --tcp-flags RST RST --dport $DPORT \
    -s $SRCIP -d $DSTIP -j DROP

  iptables -A OUTPUT -s $SRCIP -d $DSTIP \
    -p ICMP --icmp-type port-unreachable -j DROP

Usage:
    -s source IP address
    -d dest   IP address
    -p dest   port
    -q source port
    -r sends a reset (after a delay)

    -f fail-scenario
      (one SYN reuse conn, that gets dropped, but pickup by conntrack)

"""

import getopt
import sys
import time

from scapy.all import *

# Default settings:
srcip = None
dstip = None
dstport = 6666
srcport = 1337
init_seq = 100
send_reset = False
fail_scenarie = False

def fake_tcp_3WHS(srcip, dstip, src_port, dst_port, init_seq):

    ip=IP(src=srcip, dst=dstip)
    TCP_SYN=TCP(sport=src_port, dport=dst_port, flags="S", seq=init_seq)
    # Send TCP SYN packet + Recv TCP SYN-ACK packet
    TCP_SYNACK=sr1(ip/TCP_SYN)

    # Construct and send 3rd-ACK packet in 3WHS sequence
    my_ack = TCP_SYNACK.seq + 1
    my_seq = init_seq + 1
    TCP_ACK=TCP(sport=src_port, dport=dst_port, flags="A",
                seq=my_seq, ack=my_ack)
    send(ip/TCP_ACK)
    # State: ESTAB
    #
    # If SERVER closes down, goes into: FIN-WAIT-1
    #  * Server will try to send "info" (FIN-ACK) to us,
    #    but we don't listen/recv
    #  * Sending FIN+ACK (retrans 8 times, increasing timeouts)
    #
    return my_ack;

def send_data(srcip, dstip, src_port, dst_port, seq, ack):
    my_payload="AAAABBBBCCCC"
    ip=IP(src=srcip, dst=dstip)
    TCP_PUSH=TCP(sport=src_port, dport=dst_port, flags="PA", seq=seq, ack=ack)
    TCP_DATA_ACK=sr1(ip/TCP_PUSH/my_payload)
    my_seq = TCP_DATA_ACK.ack
    return my_seq

def send_fin(srcip, dstip, src_port, dst_port, seq, ack):
    ip=IP(src=srcip, dst=dstip)
    TCP_FIN=TCP(sport=src_port, dport=dst_port, flags="FA", seq=seq, ack=ack)
    send(ip/TCP_FIN)
    # New server state: LAST-ACK
    # * because we are not ACKing the servers FIN-ACK, server will
    #   retrans (Linux, 8 times) these FIN-ACKs until it closes the
    #   socket

def send_rst(srcip, dstip, src_port, dst_port, seq):
    # The RST (if correct seq) will bring the server side out of LAST-ACK
    # and into a closed state
    ip=IP(src=srcip, dst=dstip)
    TCP_RST=TCP(sport=src_port, dport=dst_port, flags="R", seq=seq, ack=0)
    send(ip/TCP_RST)

def fake_tcp_syn(srcip, dstip, src_port, dst_port, init_seq):
    ip=IP(src=srcip, dst=dstip)
    TCP_SYN2=TCP(sport=src_port, dport=dst_port, flags="S", seq=init_seq)
    # Send TCP SYN packet (ignoring reply)
    send(ip/TCP_SYN2)
    # If RFC5961 is implemented a challenge-ACK would be send back
    # containing the seq need to RST the connection, but we don't need it.


if __name__ == "__main__":
    def usage(msg=None):
        if msg: sys.stderr.write('%s: %s\n' % (sys.argv[0], msg))
        sys.stderr.write(__doc__)
        sys.exit(1)

    try:
        opts, args = getopt.getopt(sys.argv[1:], 'hrfs:d:p:q:')
        for o, a in opts:
            if o == '-h': usage()
            elif o == '-s': srcip = a
            elif o == '-d': dstip = a
            elif o == '-p': dstport = int(a)
            elif o == '-q': srcport = int(a)
            elif o == '-r': send_reset = True
            elif o == '-f': fail_scenarie = True
            else: raise Warning, 'EDOOFUS - Programming error'
    except getopt.GetoptError, e:
        usage(e)

    if not dstip:
        usage("Must specify destination (-d)")

    track_ack = fake_tcp_3WHS(srcip, dstip, srcport, dstport, init_seq)

    my_seq = init_seq + 1
    my_seq = send_data(srcip, dstip, srcport, dstport, my_seq, track_ack)
    # Start "active" close with first FIN
    send_fin(srcip, dstip, srcport, dstport, my_seq, track_ack)

    # This sends two (fake) SYN reuse conn attempt
    #
    # - If conntrack is enabled it will transition into wrong state
    #   due to TCP stack responding with an ACK to the "spurious" SYN,
    #   as required by RFC5961, but conntrack wrongly sees it as a ACK
    #   to the LAST-ACK state.  Conntrack (wrongly) transition into
    #   "TIME-WAIT" (while socket remains in "LAST-ACK").
    #
    # - On second "spurious" SYN, conntrack (wrongly) will transition
    #   from "TIME-WAIT" into "SYN_SENT" (believing this is a reopened
    #   conn), while the socket is still in "LAST-ACK".
    #
    if fail_scenarie:
        delay1=2
        print "Delay", delay1, "sec, before sending TCP fake SYN reuse conn"
        print "(If bug present, conntrack will go-into TIME_WAIT)"
        time.sleep(delay1)
        fake_tcp_syn(srcip, dstip, srcport, dstport, 65535)
        delay2=3
        print "Delay", delay2, "sec, before sending yet-another  TCP fake SYN"
        print "(If bug present, conntrack will go-into SYN_SENT)"
        time.sleep(delay2)
        fake_tcp_syn(srcip, dstip, srcport, dstport, 65535)

    if send_reset:
        delay=10
        print "Delay", delay, "sec, before sending TCP RST"
        time.sleep(delay)
        my_seq = my_seq + 1
        send_rst(srcip, dstip, srcport, dstport, my_seq)
