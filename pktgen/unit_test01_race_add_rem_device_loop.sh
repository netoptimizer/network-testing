#!/bin/bash
#
# NOTICE: THIS IS NOT A FUNCTIONAL PROGRAM
#  - Test program for race condition testing while developing pktgen
#
# Trying to kill pktgen, by having:
#  pktgen_add_device() race with pktgen_remove_device()
#
# Additional read data via /proc to make that race too
#
# Author: Jesper Dangaaard Brouer
# License: GPL
#
basedir=`dirname $0`
source ${basedir}/functions.sh
root_check_run_with_sudo "$@"
source ${basedir}/parameters.sh
source ${basedir}/config.sh

# Run this in parallel in another thread/process
function add_remove_loop() {
    while (true); do
	thread=0
	add_device $DEV $thread &
 	remove_thread $thread &
#	thread=2
#	add_device $DEV $thread
#	reset_all_threads
    done
}

function read_proc_file() {
    for i in `seq 1 10000`; do
	echo "Reading proc N:$i"
 	cat /proc/net/pktgen/kpktgend_0 &
	cat /proc/net/pktgen/$DEV@0
    done
}

function control_c()
# run if user hits control-c
{
    echo -en "\n*** Ctrl-C ***\n"
    kill $BACKGROUND_PID
    exit $?
}

# trap keyboard interrupt (control-c)
trap control_c SIGINT

add_remove_loop &
BACKGROUND_PID=$!
read_proc_file

kill $BACKGROUND_PID
