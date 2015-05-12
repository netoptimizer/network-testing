#!/bin/bash
#
# Script for playing with pktgen "burst" option (use -b $N)
#  - This avoids writing the HW tailptr on every driver xmit
#  - The performance boost is impressive, see commit link
#    If correctly tuned, single CPU 10G wirespeed small pkts is possible
#
#  Avail since:
#   commit 38b2cf2982dc73 ("net: pktgen: packet bursting via skb->xmit_more")
#   https://git.kernel.org/cgit/linux/kernel/git/davem/net-next.git/commit/?id=38b2cf2982dc73
#
basedir=`dirname $0`
source ${basedir}/functions.sh
root_check_run_with_sudo "$@"
source ${basedir}/parameters.sh
source ${basedir}/config.sh

# Base Config
DELAY="0"  # Zero means max speed
COUNT="0"  # Zero means indefinitely
[ -z "$CLONE_SKB" ] && CLONE_SKB="100000"

# Packet setup
# (example of setting default params in your script)
[ -z "$DEST_IP" ] && DEST_IP=10.10.10.1
[ -z "$DST_MAC" ] && DST_MAC=$MAC_eth61_albpd42
[ -z "$BURST" ] && BURST=0

# Threads
min=0
max=$NUM_THREADS
reset_all_threads
create_threads 0 $NUM_THREADS

for thread in `seq $min $max`; do
    dev=${DEV}@${thread}
    # FIXME: Ugly old style usage of global variable setting... should
    # fix before publishing this script...
    PGDEV=/proc/net/pktgen/$dev
    base_config

    pg_set $dev "dst $DEST_IP"
    pg_set $dev "burst $BURST"
done

start_run
