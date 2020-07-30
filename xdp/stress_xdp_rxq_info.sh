#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
#
# Stress test loading and unloading XDP BPF programs
#
# The setup for this test is to start a traffic generator, sending packets
# to Device Under Test (DUT), and then run this script on DUT.  The idea is
# to stress XDP atomic replace mecanism while processing traffic.

if [[ -z "$1" ]]; then
    echo "ERR - Must specify interface DEV as first argument"
    exit 2
fi
DEV=$1

if [[ "$EUID" -ne 0 ]]; then
    echo "ERR - Must be root to load XDP on interface $DEV"
    exit 3
fi

if [[ ! -x xdp_rxq_info ]]; then
    echo "ERR - run script from kernel/samples/bpf/ directory"
    exit 4
fi

# Default delay
DELAY=1
# Allow overwriting delays via cmdline args
DELAY1=${2:-$DELAY}
DELAY2=${3:-$DELAY}
DELAY3=${4:-$DELAY}

# trap Ctrl-c and call ctrl_c()
trap ctrl_c INT

function ctrl_c() {
    echo "** Trapped Ctrl-C -  Kill remaining backgrounded XDP programs"
    #kill $pidA
    #kill $pidB
    pkill xdp_rxq_info
    exit 0
}

i=0
while [ 1 ]; do
    ((i++))
    echo "Test-iteration $i (double forced load)"

    # This call test: loading XDP on DEV without another XDP prog
    ./xdp_rxq_info --dev $DEV --action XDP_DROP --force &
    pidA=$!
    sleep $DELAY1

    # This call test: swapping XDP prog on interface with exiting XDP pro
    ./xdp_rxq_info --dev $DEV --action XDP_DROP --force &
    pidB=$!
    sleep $DELAY2

    # Unload XDP progs via killing xdp_rxq_info (signal handler)
    kill $pidA
    kill $pidB
    sleep $DELAY3 # Give chance that other progs were unloaded
done
