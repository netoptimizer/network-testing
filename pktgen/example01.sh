#!/bin/bash
#
# Example01: Using pktgen sending on multiple CPUs
#
basedir=`dirname $0`
source ${basedir}/functions.sh
root_check_run_with_sudo "$@"
source ${basedir}/parameters.sh
source ${basedir}/config.sh

# Base Config
DELAY="0"  # Zero means max speed
COUNT="0"  # Zero means indefinitely
[ -z "$CLONE_SKB" ] && CLONE_SKB="64"

# Packet setup
UDP_MIN=9
UDP_MAX=109
# (example of setting default params in your script)
[ -z "$DEST_IP" ] && DEST_IP=10.10.10.1
[ -z "$DST_MAC" ] && DST_MAC=$MAC_eth61_albpd42

# Threads
min=0
max=$NUM_THREADS
reset_all_threads
create_threads 0 $NUM_THREADS

for thread in `seq $min $max`; do
    dev=${DEV}@${thread}
    # base config
    pg_set $dev "flag QUEUE_MAP_CPU"
    pg_set $dev "count $COUNT"
    pg_set $dev "clone_skb $CLONE_SKB"
    pg_set $dev "pkt_size $PKT_SIZE"
    pg_set $dev "delay $DELAY"
    pg_set $dev "dst_mac $DST_MAC"

    pg_set $dev "dst $DEST_IP"
    # Setup random UDP src range
    pg_set $dev "flag UDPSRC_RND"
    pg_set $dev "udp_src_min $UDP_MIN"
    pg_set $dev "udp_src_max $UDP_MAX"

done

start_run
