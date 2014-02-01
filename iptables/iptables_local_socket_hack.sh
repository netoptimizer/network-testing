#!/bin/bash
#
# Example of localhost DDoS protection without using conntrack
#  by using matching against local sockets.
#
#  (xt_socket --nowildcard support added in kernel 3.11 and iptables v1.4.21)
#
# Author: Jesper Dangaard Brouer <brouer@redhat.com>
# License: GPL
#
# This script is still work-in-progress, and not optimal yet...

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
    # TODO: Unload conntrack kernel modules
fi

# Untrack the port, we are protecting
# (... can be left out if not loading conntrack)
iptables -t raw -I PREROUTING -i $DEV -p tcp -m tcp \
    --dport $PORT -j CT --notrack

iptables -N limit_new_conns

iptables -N socket_filter
iptables -A INPUT -p tcp -m tcp --dport $PORT -j socket_filter

# ACCEPT packets matching against "established" socket
iptables -A socket_filter -m socket -j ACCEPT
#
# Fall-through, will basically be packet hitting the LISTEN lock
#  - Thus, we should limit these packets
#
# Still verify local socket is listening, but will also
# match SYN and 3WHS-ACK packets (and SYN-ACK).
# * Problem is these packet can still be fake packets
#
iptables -A socket_filter -m socket --nowildcard -j limit_new_conns

# Kernel note: __inet_lookup_listener() looks very "hot"

# Limit SYN
iptables -A limit_new_conns -p tcp -m tcp --syn \
    -m hashlimit \
    --hashlimit-above 200/sec --hashlimit-burst 1000 \
    --hashlimit-mode srcip    --hashlimit-name syn \
    --hashlimit-htable-size 2097152 \
    --hashlimit-srcmask 24 -j DROP

# Limit ACK
iptables -A limit_new_conns -p tcp -m tcp --tcp-flags FIN,SYN,RST,ACK ACK \
    -m hashlimit \
    --hashlimit-above 200/sec --hashlimit-burst 1000 \
    --hashlimit-mode srcip    --hashlimit-name ack \
    --hashlimit-htable-size 2097152 \
    --hashlimit-srcmask 24 -j DROP

# Limit SYN-ACK
iptables -A limit_new_conns -p tcp -m tcp --tcp-flags FIN,SYN,RST,ACK SYN,ACK \
    -m hashlimit \
    --hashlimit-above 200/sec --hashlimit-burst 1000 \
    --hashlimit-mode srcip    --hashlimit-name synack \
    --hashlimit-htable-size 2097152 \
    --hashlimit-srcmask 24 -j DROP

# Limit rest
#iptables -A limit_new_conns -p tcp -m tcp \
#    -m hashlimit \
#    --hashlimit-above 200/sec --hashlimit-burst 1000 \
#    --hashlimit-mode srcip    --hashlimit-name synack \
#    --hashlimit-htable-size 2097152 \
#    --hashlimit-srcmask 24 -j DROP

# Kernel note: about hashlimit, there is a scalability problem in
#  dsthash_alloc_init() because general hash table lock is taken

#     -m limit --limit 10000/sec --limit-burst 1000 -j DROP
