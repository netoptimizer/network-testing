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
CLONE_SKB="64"

# Packet setup
UDP_MIN=9
UDP_MAX=109
# (example of setting default params in your script)
[ -z "$DEST_IP" ] && DEST_IP=10.10.10.1
[ -z "$DST_MAC" ] && DST_MAC=$MAC_eth61_albpd42

# Threads
min=0
max=$NUM_THREADS
remove_threads 0 15
create_threads 0 $NUM_THREADS

for num in `seq $min $max`; do
    # FIXME: Ugly old style usage of global variable setting... should
    # fix before publishing this script...
    PGDEV=/proc/net/pktgen/${DEV}@$num
    base_config
    set_dst_ip $DEST_IP
    set_udp_src_range $UDP_MIN $UDP_MAX
done

start_run
