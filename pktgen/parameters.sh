#
# Common parameter parsing for pktgen scripts
#

function usage() {
    echo ""
    echo "Usage: $0 [-vx] [-s pkt_size]"
    echo "  -s : packet size"
    echo "  -i : output interface/device"
    echo "  -d : destination IP"
    echo "  -m : destination MAC-addr"
    echo "  -t : threads to start"
    echo "  -b : HW level bursting of SKBs" #experimental feature
    echo "  -v : verbose"
    echo "  -x : debug"
    echo ""
}

##  --- Parse command line arguments / parameters ---
## echo "Commandline options:"
while getopts "s:i:d:m:t:b:vx" option; do
    case $option in
        s)
          export PKT_SIZE=$OPTARG
	  info "Packet size set to: $PKT_SIZE bytes"
          ;;
        d) # destination IP
          export DEST_IP=$OPTARG
	  info "Destination IP set to: $DEST_IP"
          ;;
        i) # interface
          export DEV=$OPTARG
	  info "Output device set to: $DEV"
          ;;
        m) # MAC
          export DST_MAC=$OPTARG
	  info "Destination MAC set to: $DST_MAC"
          ;;
        t)
          export NUM_THREADS=$OPTARG
	  let "NUM_THREADS -= 1"
	  info "Number of threads to start: $OPTARG (0 to $NUM_THREADS)"
          ;;
        b)
          export BURST=$OPTARG
	  info "Experimental SKB bursting: $OPTARG"
          ;;
        v)
          info "- Verbose mode -"
          export VERBOSE=yes
          ;;
        x)
          info "- Debug mode -"
          export DEBUG=yes
          ;;
        ?|*)
          usage;
          err 2 "[ERROR] Unknown parameters!!!"
    esac
done
#shift $[ OPTIND - 1 ]
shift $(( $OPTIND - 1 ))

if [ -z "$PKT_SIZE" ]; then
    export PKT_SIZE=60
    info "Default packet size set to: set to: $PKT_SIZE bytes"
fi

if [ -z "$NUM_THREADS" ]; then
    # Zero threads means one thread, because CPU numbers are zero indexed
    export NUM_THREADS=0
fi

if [ -z "$DEV" ]; then
    usage
    err 2 "Please specify output device"
fi

if [ -z "$DST_MAC" ]; then
    warn "Missing destination MAC address"
fi

if [ -z "$DEST_IP" ]; then
    warn "Missing destination IP address"
fi
