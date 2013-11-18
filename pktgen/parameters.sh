#
# Common parameter parsing for pktgen scripts
#

function usage() {
    echo ""
    echo "Usage: $0 [-dv] [-s pkt_size]"
    echo "  -v : verbose"
    echo "  -d : debug"
    echo "  -s : packet size"
    echo ""
}

##  --- Parse command line arguments / parameters ---
## echo "Commandline options:"
while getopts "s:dv" option; do
    case $option in
        s)
          echo " Packet size: \"$OPTARG\""
          export PKT_SIZE=$OPTARG
	  echo "Packet size set to: set to: $PKT_SIZE bytes"
          ;;
        v)
          echo "- Verbose mode -"
          export VERBOSE=yes
          ;;
        d)
          echo "- Debug mode -"
          export DEBUG=yes
          ;;
        ?|*)
          echo "[ERROR] Unknown parameters!!!"
          usage;
          exit 2
    esac
done
#shift $[ OPTIND - 1 ]
shift $(( $OPTIND - 1 ))

if [ -z "$PKT_SIZE" ]; then
    export PKT_SIZE=1500
    echo "Default packet size set to: set to: $PKT_SIZE bytes"
fi
