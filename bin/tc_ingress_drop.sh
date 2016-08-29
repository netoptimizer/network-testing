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
    echo "Usage: $0 [-vfh] --dev ethX"
    echo "  -d | --dev     : (\$DEV)       Ingress interface/device (required)"
    echo "  -v | --verbose : (\$VERBOSE)   verbose"
    echo "  --flush        : (\$FLUSH)     Only flush (remove TC drop rules)"
    echo "  --dry-run      : (\$DRYRUN)    Dry-run only (echo tc commands)"
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
function _call_tc() {
    local allow_fail="$1"
    shift
    if [[ -n "$VERBOSE" ]]; then
	echo "tc $@"
    fi
    if [[ -n "$DRYRUN" ]]; then
	return
    fi
    $TC "$@"
    local status=$?
    if (( $status != 0 )); then
	if [[ "$allow_fail" == "" ]]; then
	    err 3 "Exec error($status) occurred cmd: \"$TC $@\""
	fi
    fi
}
function call_tc() {
    _call_tc "" "$@"
}
function call_tc_allow_fail() {
    _call_tc "allow_fail" "$@"
}


# Using external program "getopt" to get --long-options
OPTIONS=$(getopt -o vfshd: \
    --long verbose,dry-run,flush,stats,dev: -- "$@")
if (( $? != 0 )); then
    err 2 "Error calling getopt"
fi
eval set -- "$OPTIONS"

##  --- Parse command line arguments / parameters ---
while true; do
    case "$1" in
        -d | --dev ) # device
          export DEV=$2
	  info "Output device set to: DEV=$DEV" >&2
	  shift 2
          ;;
        -v | --verbose)
          export VERBOSE=yes
          # info "Verbose mode: VERBOSE=$VERBOSE" >&2
	  shift
          ;;
        --dry-run )
          export DRYRUN=yes
          export VERBOSE=yes
          info "Dry-run mode: enable VERBOSE and don't call TC" >&2
	  shift
          ;;
        -f | --flush )
          export FLUSH=yes
	  shift
          ;;
        -s | --stats )
          export STATS_ONLY=yes
	  shift
          ;;
	-- )
	  shift
	  break
	  ;;
        -h )
          usage;
          err 4 "[ERROR] Unknown parameters!!!"
	  ;;
	* )
	  shift
	  break
	  ;;
    esac
done

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
    call_tc_allow_fail qdisc del dev $device ingress
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
    info "Display filter results with stats:"
    call_tc -s filter ls dev $device parent ffff: protocol ip
}

function tc_ingress_stat2()
{
    local device="$1"
    info "Display filter results with stats:"
    call_tc -s actions ls action gact
}

if [[ -n "$FLUSH" ]]; then
    info "Clearing TC ingress drop rules"
    call_tc_allow_fail qdisc del dev $DEV ingress
    exit 0
fi

if [[ -n "$STATS_ONLY" ]]; then
    tc_ingress_stat1 $DEV
    #tc_ingress_stat2 $DEV
    exit 0
fi


tc_ingress_flush $DEV
tc_ingress_drop_all $DEV
