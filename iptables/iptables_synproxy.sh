#!/bin/bash
#
#  iptables SYNPROXY target usage example
#  (support added in iptables v1.4.21)
#
# Author: Jesper Dangaard Brouer <brouer@redhat.com>

#export IPTABLES_CMD=
default_ipt_cmd="/usr/local/sbin/iptables"

if [ "$EUID" -ne 0 ]; then
    # Can be run as normal user, will just use "sudo"
    export su=sudo
fi

function usage() {
    echo ""
    echo " $0 - SYNPROXY setup script"
    echo ""
    echo "Usage:"
    echo "------"
    echo " Script    : $0"
    echo " Parameters: [-vf] -i interface -p dest-port"
    echo ""
    echo "  -v : verbose"
    echo "  -i : Interface/device"
    echo "  -p : Destination TCP port"
    echo "  -f : Flush rules before creating new rules"
    echo ""
}

##  --- Parse command line arguments ---
while getopts ":i:p:vf" option; do
    case $option in
        i)
            DEV=$OPTARG
            ;;
        p)
            PORT=$OPTARG
            ;;
        v)
            VERBOSE=yes
            ;;
        f)
            FLUSH=yes
            ;;
        ?|*)
            echo ""
            echo "[ERROR] Unknown parameter \"$OPTARG\""
            usage
            exit 2
    esac
done
shift $[ OPTIND - 1 ]

if [ -z "$DEV" ]; then
    usage
    echo "ERROR: no device specified"
    exit 1
fi

if [ -z "$PORT" ]; then
    usage
    echo "ERROR: no port specified"
    exit 1
fi

# Extra checking for iptables
if [ -z "$IPTABLES_CMD" ]; then
    echo "WARNING: Shell env variable IPTABLES_CMD is undefined"
    export IPTABLES_CMD=${default_ipt_cmd}
    echo "WARNING: Fallback to default IPTABLES_CMD=${default_ipt_cmd}"
fi

#
# A shell iptables function wrapper
#
iptables() {
    $su $IPTABLES_CMD "$@"
    local result=$?
    if [ ${result} -gt 0 ]; then
        echo "WARNING -- Error (${result}) when executing the iptables command:"
        echo " \"iptables $@\""
    else
        if [ -n "${VERBOSE}" ]; then
            echo "iptables $@"
        fi
    fi
}

# Cleanup before applying our rules
if [ -n "$FLUSH" ]; then
    iptables -t raw -F
    iptables -t raw -X
    iptables -F
    iptables -X
fi

# SYNPROXY works on untracked conntracks
#  it will create the appropiate conntrack proxied TCP conn
# NOTICE: table "raw"
iptables -t raw -I PREROUTING -i $DEV -p tcp -m tcp --syn \
    --dport $PORT -j CT --notrack

# Catching state
#  UNTRACKED == SYN packets
#  INVALID   == ACK from 3WHS
iptables -A INPUT -i $DEV -p tcp -m tcp --dport $PORT \
    -m state --state INVALID,UNTRACKED \
    -j SYNPROXY --sack-perm --timestamp --wscale 7 --mss 1460

# Drop rest of state INVALID
#  This will e.g. catch SYN-ACK packet attacks
iptables -A INPUT -i $DEV -p tcp -m tcp --dport $PORT \
    -m state --state INVALID -j DROP

# More strict conntrack handling to get unknown ACKs (from 3WHS) to be
#  marked as INVALID state (else a conntrack is just created)
#
$su /sbin/sysctl -w net/netfilter/nf_conntrack_tcp_loose=0

# Enable timestamping, because SYN cookies uses TCP options field
$su /sbin/sysctl -w net/ipv4/tcp_timestamps=1

# Adjusting maximum number of connection tracking entries possible
#
# Conntrack element size 288 bytes found in /proc/slabinfo
#  "nf_conntrack" <objsize> = 288
#
# 288 * 2000000 / 10^6 = 576.0 MB
$su /sbin/sysctl -w net/netfilter/nf_conntrack_max=2000000

# IMPORTANT: Also adjust hash bucket size for conntracks
#   net/netfilter/nf_conntrack_buckets writeable
#   via /sys/module/nf_conntrack/parameters/hashsize
#
# Hash entry 8 bytes pointer (uses struct hlist_nulls_head)
#  8 * 2000000 / 10^6 = 16 MB
$su sh -c 'echo 2000000 > /sys/module/nf_conntrack/parameters/hashsize'

# Hint: Monitor nf_conntrack usage searched, found, new, etc.:
#  lnstat -c -1 -f nf_conntrack
