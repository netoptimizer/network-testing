#
# Common parameter parsing for pktgen scripts
#

function usage() {
    echo ""
    echo "Usage: $0 [-vx] [-s pkt_size]"
    echo "  -s : packet size"
    echo "  -d : output device"
    echo "  -m : destination MAC-addr"
    echo "  -t : threads to start"
    echo "  -v : verbose"
    echo "  -x : debug"
    echo ""
}

##  --- Parse command line arguments / parameters ---
## echo "Commandline options:"
while getopts "s:d:m:t:vx" option; do
    case $option in
        s)
          export PKT_SIZE=$OPTARG
	  info "Packet size set to: $PKT_SIZE bytes"
          ;;
        d) # device
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
    export PKT_SIZE=1500
    info "Default packet size set to: set to: $PKT_SIZE bytes"
fi

if [ -z "$DEV" ]; then
    usage
    err 2 "Please specify output device"
fi
