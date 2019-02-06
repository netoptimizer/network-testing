#!/bin/bash
# Example script for how to solve qdisc locking issue when shaping traffic. Can
# be used in cases where global rate limiting it not the goal, but instead the
# goal is to rate limit customers or services (to something significantly lower
# than NIC link speed).
#
# Basic solution:
#  - Use MQ which have multiple transmit queues (TXQ).
#  - For each MQ TXQ assign a HTB qdisc
#
basedir=`dirname $0`
source ${basedir}/functions.sh
export TC=tc

VERBOSE=y

root_check_run_with_sudo "$@"

# Global setup variables
#
# Each of the HTB root-class(es) get these RATE+CEIL upper bandwidth bounds.
ROOT_RATE=2500Mbit
ROOT_CEIL=3000Mbit
#
# The default HTB class
DEF_RATE=100Mbit
DEF_CEIL=150Mbit

DEV=$1
if [[ -z "$DEV" ]]; then
    err 1 "Must specify DEV as argument"
fi

info "Applying TC setup on device: $DEV"

# Can see how many TXQs a device have via directories:
#  /sys/class/net/<interface>/queues

# Try to detect if HW can be used (as the TC error message is useless)
if [[ ! -e /sys/class/net/$DEV/queues/tx-1 ]]; then
    err 2 "The device ($DEV) must have multiple TX hardware queue.\n" \
	  "The MQ qdisc only works to multi-queue capable hardware"
fi

# Debug: Show script executing
# set -xv

info "Clear existing setup"
call_tc_allow_fail qdisc del dev $DEV root

# Script dies if any command fails
set -e

info " New MQ, with larger handle (MAJOR:) to allow HTB qdisc to use major 1:"
call_tc qdisc replace dev $DEV root handle 7FFF: mq

info "Foreach TXQ - create HTB leaf(s) under MQ 0x7FFF:TXQ"
i=0
for dir in /sys/class/net/$DEV/queues/tx-*; do
    ((i++)) || true
    # Qdisc HTB $i: under parent 7FFF:$i
    call_tc   qdisc add dev $DEV parent 7FFF:$i handle $i: htb default 2
    # tc qdisc add dev $DEV parent 7FFF:1  handle 1:  htb default 2
    # tc qdisc add dev $DEV parent 7FFF:2  handle 2:  htb default 2
    # tc qdisc add dev $DEV parent 7FFF:3  handle 3:  htb default 2
    # tc qdisc add dev $DEV parent 7FFF:4  handle 4:  htb default 2
done

# Create root-CLASS(es) under each HTB-qdisc
info "Create HTB root-class(es) n:1 (rate $ROOT_RATE ceil $ROOT_CEIL)"
info " - Also create HTB default class n:2"
i=0
for dir in /sys/class/net/$DEV/queues/tx-*; do
    ((i++)) || true
    # The root-class set upper bandwidth usage
    call_tc class add dev $DEV parent $i: classid $i:1 \
       htb rate $ROOT_RATE ceil $ROOT_CEIL

    # Create HTB default class $i:2
    call_tc class add dev $DEV parent $i:1 classid $i:2 \
       htb rate $DEF_RATE ceil $DEF_CEIL

    [[ -n "$VERBOSE" ]] && echo ""
    # Should we also change the qdisc default HTB class $i:2 ?
    # tc qdisc add dev $DEV parent $i:2 sfq
    # tc qdisc add dev $DEV parent $i:2 fq_codel
done

info "Now create services/customers bandwidth limits"
# Simple example:
call_tc class add dev $DEV parent 2:1  classid 2:42 htb rate 2Mbit ceil 3Mbit
call_tc qdisc add dev $DEV parent 2:42 sfq

set -v
#
# ***NOTICE*** YOU ARE NOT DONE
#
# Getting services/customers correctly categorised is the next challenge that is
# currently left as an exercise...
#
# For solving the TX-queue locking congestion, the traffic needs to be
# redirected to the appropriate CPUs. This can either be done with RSS (Receive
# Side Scaling) and RPS (Receive Packet Steering), or with XDP cpumap redirect.
#

# Hint: this script is part of my testing of CPUMAP XDP-redirect
