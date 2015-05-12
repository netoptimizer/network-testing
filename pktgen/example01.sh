#!/bin/bash
#
# Example01: Using pktgen sending on multiple CPUs
#
basedir=`dirname $0`
source ${basedir}/functions.sh
root_check_run_with_sudo "$@"
source ${basedir}/parameters.sh

# Base Config
DELAY="0"       # Zero means max speed
COUNT="100000"  # Zero means indefinitely
[ -z "$CLONE_SKB" ] && CLONE_SKB="64"

# Packet setup
UDP_MIN=9
UDP_MAX=109
# (example of setting default params in your script)
[ -z "$DEST_IP" ] && DEST_IP="198.18.0.42"
[ -z "$DST_MAC" ] && DST_MAC="90:e2:ba:ff:ff:ff"

# General cleanup everything since last run
pg_ctrl "reset"

# Threads are specified with parameter -t value in $NUM_THREADS
for thread in `seq 0 $NUM_THREADS`; do
    dev=${DEV}@${thread}

    # Add remove all other devices and $dev to thread
    pg_thread $thread "rem_device_all"
    pg_thread $thread "add_device" $dev

    # Base config of dev
    pg_set $dev "flag QUEUE_MAP_CPU"
    pg_set $dev "count $COUNT"
    pg_set $dev "clone_skb $CLONE_SKB"
    pg_set $dev "pkt_size $PKT_SIZE"
    pg_set $dev "delay $DELAY"

    # Destination
    pg_set $dev "dst_mac $DST_MAC"
    pg_set $dev "dst $DEST_IP"

    # Setup random UDP src range
    pg_set $dev "flag UDPSRC_RND"
    pg_set $dev "udp_src_min $UDP_MIN"
    pg_set $dev "udp_src_max $UDP_MAX"
done

# start_run
echo "Running... ctrl^C to stop" >&2
pg_ctrl "start"
echo "Done" >&2

for thread in `seq 0 $NUM_THREADS`; do
    dev=${DEV}@${thread}
    echo "Device: $dev"
    cat /proc/net/pktgen/$dev | grep -A2 "Result:"
done
