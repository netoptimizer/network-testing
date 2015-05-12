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

# Detailed description of race
# ============================
# (race requires understanding pktgen's thread model, desc below)
#
# In pktgen there is several kernel threads, but there is only one CPU
# running each kernel thread.  Communication with the kernel threads are
# done through some thread control flags.  This allow the thread to
# change data structures at a know synchronization point, see main
# thread func pktgen_thread_worker().
#
# Userspace changes are communicated through proc-file writes.  There
# are three types of changes, general control changes "pgctrl"
# (func:pgctrl_write), thread changes "kpktgend_X"
# (func:pktgen_thread_write), and interface config changes "etcX@N"
# (func:pktgen_if_write).
#
# Userspace "pgctrl" and thread changes are synchronized via the mutex
# pktgen_thread_lock, thus only a single userspace instance can run.
# The mutex is taken while the packet generator is running, by pgctrl
# "start".  Thus e.g. "add_device" cannot be invoked when pktgen is
# running/started.
#
# All "pgctrl" and all "thread" changes, except thread "add_device",
# communicate via the thread control flags.  The main problem is the
# exception "add_device", that modifies threads "if_list" directly.
#
# (race)
# Fortunately "add_device" cannot be invoked while pktgen is running.
# But there exists a race between "rem_device_all" and "add_device"
# (which normally don't occur, because "rem_device_all" waits 125ms
# before returning). Background'ing "rem_device_all" and running
# "add_device" immediately allow the race to occur.
#
# The race affects the threads (list of devices) "if_list".  The if_lock
# is used for protecting this "if_list".  Other readers are given
# lock-free access to the list under RCU read sections.
#
# (possible extra race)
# Note, interface config changes (via proc) can occur while pktgen is
# running, which worries me a bit.  I'm assuming proc_remove() takes
# appropriate locks, to assure no writers exists after proc_remove()
# finish.

# run if user hits control-c
function control_c()
{
    echo -en "\n*** Ctrl-C ***\n"
    kill $BACKGROUND_PID
    exit $?
}
# trap keyboard interrupt (control-c)
trap control_c SIGINT

function write_to_interface() {
    # Write something to proc file to test for races
    # when the proc file gets removed while writing
    echo "pkt_size $RANDOM" > /proc/net/pktgen/$DEV@0
}

# Run this in parallel in another thread/process
function add_remove_loop() {
    while (true); do
	thread=0
	pg_thread $thread "rem_device_all" &
	sleep 0.05
	pg_thread $thread "add_device" $DEV
	write_to_interface
	wait $!
    done
}

function read_proc_file() {
    for i in `seq 1 1000`; do
	echo -e "\n--- Reading proc files in parallel N:$i ---"
 	cat /proc/net/pktgen/kpktgend_0
	if [ -e /proc/net/pktgen/$DEV@0 ]; then
	    write_to_interface
	    echo "READING interface file (to /dev/null)"
	    cat /proc/net/pktgen/$DEV@0 > /dev/null
	else
	    echo "*** INTERFACE FILE DISAPPEARED ***"
	fi
	sleep 1
    done
}

add_remove_loop &
BACKGROUND_PID=$!
read_proc_file

kill $BACKGROUND_PID
