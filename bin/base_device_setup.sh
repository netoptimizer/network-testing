#!/bin/bash
# Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>

if [ -z "$1" ]; then
    echo -e "ERROR: Usage: $0 device"
    exit
fi
DEV=$1

set -x
sudo killall irqbalance
sudo ~/bin/set_irq_affinity $DEV
sudo ethtool -A $DEV rx off tx off autoneg off
