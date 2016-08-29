#!/bin/bash
#
# This script drop ingress packets on an interface very early in Linux
# network stack using the TC (Traffic Control) ingress hook.
#
# The primarily purpose is for doing "zoom-in" benchmarking on the
# early RX path of the kernel, to find bottlenecks (e.g. in the memory
# allocator).
#
# TC commands based on input from: Jamal Hadi Salim <jhs@mojatatu.com>
#
# Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>
# License: GPLv2

export TC=/sbin/tc

# sudo trick
function root_check_run_with_sudo() {
    # Trick so, program can be run as normal user, will just use "sudo"
    #  call as root_check_run_as_sudo "$@"
    if [ "$EUID" -ne 0 ]; then
	if [ -x $0 ]; then # Directly executable use sudo
	    echo "# (Not root, running with sudo)" >&2
            sudo "$0" "$@"
            exit $?
	fi
	echo "cannot perform sudo run of $0"
	exit 1
    fi
}
root_check_run_with_sudo "$@"

function usage() {
    echo ""
    echo "Usage: $0 [-vd] -i ethX"
    echo "  -i : (\$DEV)       ingress interface/device (required)"
    echo "  -f : (\$FLUSH)     Only flush (remove TC drop rules)"
    echo "  -v : (\$VERBOSE)   verbose"
    echo "  -d : (\$DRYRUN)    dry-run only (echo tc commands)"
    echo ""
}

## -- General shell logging cmds --
function err() {
    local exitcode=$1
    shift
    echo "ERROR: $@" >&2
    exit $exitcode
}

function warn() {
    echo "WARN : $@" >&2
}

function info() {
    if [[ -n "$VERBOSE" ]]; then
	echo "# $@"
    fi
}

# Wrapper call for TC
function call_tc() {
    if [[ -n "$VERBOSE" ]]; then
	echo "tc $@"
    fi
    if [[ -n "$DRYRUN" ]]; then
	return
    fi
    $TC "$@"
    local status=$?
    if (( $status != 0 )); then
	err 3 "Exec error($status) occurred cmd: \"$TC $@\""
    fi
}

##  --- Parse command line arguments / parameters ---
while getopts "i:vdfh" option; do
    case $option in
        i) # interface
          export DEV=$OPTARG
	  info "Output device set to: DEV=$DEV" >&2
          ;;
        v)
          export VERBOSE=yes
          info "Verbose mode: VERBOSE=$VERBOSE" >&2
          ;;
        d)
          export DRYRUN=yes
          export VERBOSE=yes
          info "Dry-run mode: enable VERBOSE and don't call TC" >&2
          ;;
        f)
          export FLUSH=yes
          ;;
        h|?|*)
          usage;
          err 2 "[ERROR] Unknown parameters!!!"
    esac
done
shift $(( $OPTIND - 1 ))


if [ -z "$DEV" ]; then
    usage
    err 2 "Please specify TC ingress device"
fi

function tc_ingress_flush()
{
    local device="$1"
    shift
    info "Flush existing ingress qdisc on device :$device"
    # Delete existing ingress qdisc - flushes all filters/actions
    call_tc qdisc del dev $device ingress
    # re-add ingress
    call_tc qdisc add dev $device ingress
}

function tc_ingress_drop_icmp()
{
    local device="$1"
    shift
    # Simple rule to drop all icmp
    call_tc filter add dev $device parent ffff: prio 4 protocol ip \
	u32 match ip protocol 1 0xff flowid 1:1 \
	action drop
}

function tc_ingress_drop_all()
{
    local device="$1"
    shift
    # other type of filters if you want to compare instead of above
    #
    # a) drop all
    info "Simply drop all ingress packets on device: $device"
    call_tc filter add dev $device parent ffff: prio 2 protocol ip \
	u32 match u32 0 0 flowid 1:1 \
	action drop
}

function tc_ingress_drop_ip()
{
    local device="$1"
    local ip="$2"

    #b) drop if src is XXX
    call_tc filter add dev $device parent ffff: prio 2 protocol ip \
	u32 match ip src $ip flowid 1:1 \
	action drop
}

function tc_ingress_stat1()
{
    local device="$1"
    # And display filter results with stats:
    call_tc -s filter ls dev $device parent ffff: protocol ip
}

function tc_ingress_stat2()
{
    local device="$1"
    # And display filter results with stats:
    call_tc -s actions ls action gact
}

if [[ -n "$FLUSH" ]]; then
    info "Clearing TC ingress drop rules"
    tc_ingress_flush $DEV
    exit 0
fi

tc_ingress_flush $DEV
tc_ingress_drop_all $DEV

tc_ingress_stat1 $DEV
#tc_ingress_stat2 $DEV
