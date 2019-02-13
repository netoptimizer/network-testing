#!/bin/bash
#
# Script to get an overview of how XPS mapping is configured
# Author: Jesper Dangaaard Brouer <netoptimizer@brouer.com>
# License: GPLv2

DEV=$1
if [[ -z "$DEV" ]]; then
    err 1 "Must specify DEV as argument"
fi

# Convert a mask to a list of CPUs this cover
function mask_to_cpus() {
    local mask=$1
    local cpu=0

    printf "CPUs in MASK=0x%02X =>" $mask
    while [ $mask -gt 0 ]; do
	if [[ $((mask & 1)) -eq 1 ]]; then
	    echo -n " cpu:$cpu"
	fi
	let cpu++
	let mask=$((mask >> 1))
    done
}

function cpu_to_mask() {
    local cpu=$1
    printf "%X" $((1 << $cpu))
}

set -v
# Simple grep to show xps_cpus mapping:
grep -H . /sys/class/net/$DEV/queues/tx-*/xps_cpus

# Listing that convert the MASK to CPUs
set +v
txq=0
for xps_cpus in /sys/class/net/$DEV/queues/tx-*/xps_cpus; do
    let txq++
    mask=$(cat $xps_cpus)
    value=$((0x$mask))
    #echo MASK:0x$mask
    txt=$(mask_to_cpus $value)
    echo NIC=$DEV TXQ:$txq use $txt
done
