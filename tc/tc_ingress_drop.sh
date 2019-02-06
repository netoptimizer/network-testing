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
#
basedir=`dirname $0`
source ${basedir}/functions.sh

export TC=/sbin/tc

root_check_run_with_sudo "$@"

function usage() {
    echo ""
    echo "Usage: $0 [-vfh] --dev ethX"
    echo "  -d | --dev     : (\$DEV)       Ingress interface/device (required)"
    echo "  -v | --verbose : (\$VERBOSE)   verbose"
    echo "  --flush        : (\$FLUSH)     Only flush (remove TC drop rules)"
    echo "  --dry-run      : (\$DRYRUN)    Dry-run only (echo tc commands)"
    echo "  -s | --stats   : (\$STATS)     Call TC statistics command"
    echo "  --port         : (\$UDP_PORT)  Drop given UDP port"
    echo "  --ip           : (\$IPADDR)    Drop given IP-addr"
    echo "  --icmp         : (\$ICMP)      Drop all ICMP traffic"
    echo ""
}

# Using external program "getopt" to get --long-options
OPTIONS=$(getopt -o vfshd: \
    --long verbose,dry-run,flush,stats,icmp,help,dev:,port:,ip: -- "$@")
if (( $? != 0 )); then
    usage
    err 2 "Error calling getopt"
fi
eval set -- "$OPTIONS"

##  --- Parse command line arguments / parameters ---
while true; do
    case "$1" in
        -d | --dev ) # device
          export DEV=$2
	  info "Ingress device set to: DEV=$DEV" >&2
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
        --port )
          export UDP_PORT=$2
	  export NO_DEFAULT_DROP="defined"
	  #info "Port set to: UDP_PORT=$UDP_PORT" >&2
	  shift 2
          ;;
        --ip )
          export IPADDR=$2
	  export NO_DEFAULT_DROP="defined"
	  shift 2
          ;;
        --icmp )
          export ICMP=yes
	  export NO_DEFAULT_DROP="defined"
	  shift 1
          ;;
	-- )
	  shift
	  break
	  ;;
        -h | --help )
          usage;
	  exit 0
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

function tc_ingress_drop_udp()
{
    local device="$1"
    local udp_port="$2"
    shift 2
    digit='^[0-9]+$'
    if ! [[ $udp_port =~ $digit ]] ; then
	err 5 "input error UDP port must be a digit"
    fi
    # Simple rule to drop specific UDP port
    #
# WARNING: below rule does not seem to work, because "implicit"
# nexthdr which "udp" depend on is not set.
#
#   call_tc filter add dev $device parent ffff: prio 4 protocol ip \
#	u32 \
#	match ip protocol 17 0xff \
#	match udp dst $udp_port 0xffff \
#	flowid 1:1 \
#	action drop
#
# This works, by manually setting offset for matching UDP header,
# assuming no IP-options thus 20 bytes IP-header
#
#    call_tc filter add dev $device parent ffff: prio 4 protocol ip \
#	u32 \
#	match ip protocol 17 0xff \
#	match udp dst $udp_port 0xffff at 21\
#	flowid 1:1 \
#	action drop
#
# Notice how below match use "ip dport" instead of "udp" per
# recommentation by Jamal. Further reading: man page tc-u32(8) does
# contain a warning about using this.
#
    call_tc filter add dev $device parent ffff: prio 4 protocol ip \
	u32 \
	match ip protocol 17 0xff \
	match ip dport $udp_port 0xffff \
	flowid 1:1 \
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
    shift 2

    #b) drop dst IP
    call_tc filter add dev $device parent ffff: prio 2 protocol ip \
	u32 match ip dst $ip flowid 1:1 \
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

# Default always flush existing rules
tc_ingress_flush $DEV

# Apply options selected drop rules
if [[ -n "$UDP_PORT" ]]; then
    tc_ingress_drop_udp $DEV "$UDP_PORT"
fi
if [[ -n "$ICMP" ]]; then
    tc_ingress_drop_icmp $DEV
fi
if [[ -n "$IPADDR" ]]; then
    tc_ingress_drop_ip $DEV $IPADDR
fi


# Only default drop all if not deselected by above options
if [[ -z "$NO_DEFAULT_DROP" ]]; then
    tc_ingress_drop_all $DEV
fi
