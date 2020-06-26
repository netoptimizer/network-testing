#!/bin/bash
#
# Create drop filter for nftables using 'raw' hook.
#
#  Drop UDP port 9
#
# Author: Jesper Dangaard Brouer <brouer@redhat.com>
# License: GPLv2
#
basedir=`dirname $0`
source ${basedir}/functions.sh

# export VERBOSE=1
export LIST=1

function usage() {
    echo ""
    echo "Usage: $0 [-vfh] --dev ethX"
    echo "  -v | --verbose : (\$VERBOSE)   verbose"
    echo "  --flush        : (\$FLUSH)     Only flush (remove all nftables rules)"
    echo "  --dry-run      : (\$DRYRUN)    Dry-run only (echo commands)"
    echo ""
}

# Using external program "getopt" to get --long-options
OPTIONS=$(getopt -o vfhd: \
    --long verbose,dry-run,flush,help -- "$@")
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
          info "Dry-run mode: enable VERBOSE and don't call nft" >&2
	  shift
          ;;
        -f | --flush )
          export FLUSH=yes
	  shift
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

if [ "$EUID" -ne 0 ]; then
    # Can be run as normal user, will just use "sudo"
    export su=sudo
fi


if [[ -n "$FLUSH" ]]; then
    echo "Clearing all nftables rules"
    nft flush ruleset
    exit 0
fi

# 0. Flush/delete entire ruleset
# ------------------------------
nft flush ruleset

# 1. Create a named 'table'
# -------------------------
# 'table' refers to a container of 'chains' with no specific semantics
#
# Syntax:
#  nft (add | delete | flush) table [<family>] <name>
nft add table ip raw4

# 2. Create base chain
# --------------------
# 'chain' within a 'table' refers to a container of rules
#
# NF_IP_PRI_RAW (-300)
# NF_IP_PRI_CONNTRACK_DEFRAG (-400)
#
nft add chain ip raw4 basechain0 \
    "{ type filter hook prerouting priority -300; }"

# 3. Create chain for our rules and connect to basechain0
# -------------------------------------------------------
nft add chain ip raw4 pktgen0
nft add rule  ip raw4 basechain0 jump pktgen0

# 4. Add rules to chain 'pktgen0'
# -------------------------------
#nft add rule ip raw4 pktgen0 ip daddr 10.40.40.2 drop
#
nft add rule ip raw4 pktgen0 udp dport 9 drop

# Show entire ruleset:
#  $ nft list ruleset
if [[ -n "$LIST" ]]; then
    nft list ruleset
fi
