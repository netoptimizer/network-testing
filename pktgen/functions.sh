#
# Common functions used by pktgen scripts
#

if [ ! -d /proc/net/pktgen ]; then
        modprobe pktgen
fi

## -- Generic proc commands -- ##

function pgset() {
    local result

    if [ "$DEBUG" == "yes" ]; then
	echo "cmd: $1 > $PGDEV"
    fi
    echo $1 > $PGDEV

    result=`cat $PGDEV | fgrep "Result: OK:"`
    if [ "$result" = "" ]; then
         cat $PGDEV | fgrep Result:
    fi
}

export PROC_DIR=/proc/net/pktgen

# More generic replacement for pgset(), that does not depend on global
# variable for proc file.
#
function proc_cmd() {
    local result
    local proc_file=$1
    # after shift, the remaining args are contained in $@
    shift
    local proc_ctrl=${PROC_DIR}/$proc_file
    if [ ! -e "$proc_ctrl" ]; then
	echo "- ERROR - proc file:$proc_ctrl does not exists!"
	exit 1
    else
	if [ ! -w "$proc_ctrl" ]; then
	    echo "- ERROR - proc file:$proc_ctrl not writable, not root?!"
	    exit 1
	fi
    fi

    if [ "$DEBUG" == "yes" ]; then
	echo "cmd: $@ > $proc_ctrl"
    fi
    # Quoting of "$@" is important for space expansion
    echo "$@" > "$proc_ctrl"

    # FIXME: Why "fgrep"
    result=`cat $proc_ctrl | fgrep "Result: OK:"`
    # FIXME: Use the shell $? exit code instead
    if [ "$result" = "" ]; then
	echo " - WARNING - failed pktgen cmd: $@ > $proc_ctrl"
        cat $proc_ctrl | fgrep Result:
    fi
}

function thread_cmd() {
    local thread=$1
    local proc_file="kpktgend_${thread}"
    shift
    proc_cmd ${proc_file} "$@"
}

function pgctrl_cmd() {
    local proc_file="pgctrl"
    proc_cmd ${proc_file} "$@"
}

function dev_cmd() {
    local dev=$1
    local thread=$1
    local proc_file=""
    shift
    proc_cmd ${proc_file} "$@"
}

## -- Pgcontrol commands -- ##

function start_run() {
    echo "Running... ctrl^C to stop"
    pgctrl_cmd "start"
    echo "Done"

}

## -- Thread control commands -- ##

function remove_thread() {
    if [ -z "$1" ]; then
	echo "[$FUNCNAME] needs thread number"
	exit 1;
    fi
    local num="$1"

    PGDEV=/proc/net/pktgen/kpktgend_${num}
    echo "[$FUNCNAME] Removing all devices from thread $PGDEV"
    pgset "rem_device_all"
}

function remove_threads() {
    if [ -z "$2" ]; then
        echo "[$FUNCNAME] range: min and max"
        exit 2
    fi
    local min=$1
    local max=$2

    for num in `seq $min $max`; do
	remove_thread ${num}
    done
}

function add_device() {
    if [ -z "$1" ]; then
	echo "[$FUNCNAME] needs device arg"
	exit 1;
    fi
    local dev="$1"

    echo "Removing all devices from thread $PGDEV"
    pgset "rem_device_all"
    echo "Adding ${dev}"
    pgset "add_device ${dev}"
}

function create_thread() {
    if [ -z "$1" ]; then
        echo "[$FUNCNAME] require input dev (optional) and number"
        exit 2
    fi
    local dev=$1
    local num=$2

    PGDEV=/proc/net/pktgen/kpktgend_${num}
    if [ -n "$num" ]; then
	add_device "${dev}@${num}"
    else
	add_device "${dev}"
    fi
}

function create_threads() {
    if [ -z "$2" ]; then
        echo "[$FUNCNAME] require input dev, (thread-range) min and max"
        exit 2
    fi
    local dev=$1
    local min=$2
    local max=$3

    for num in `seq $min $max`; do
	create_thread ${dev} ${num}
    done
}

## -- Device commands -- ##

# Common config for a dev
function base_config() {
    if [ -n "$1" ]; then
        PGDEV="$1"
    fi
    echo "Base config of $PGDEV"
    pgset "count $COUNT"
    pgset "clone_skb $CLONE_SKB"
    pgset "pkt_size $PKT_SIZE"
    pgset "delay $DELAY"
    pgset "flag QUEUE_MAP_CPU"
    pgset "dst_mac ${DST_MAC}"
}

function set_dst_ip() {
    if [ -n "$1" ]; then
	local IP="$1"
	echo "- Destination IP:$IP"
	pgset "dst $IP"
    else
	echo "[$FUNCNAME] input error"
	exit 2
    fi
}

# Input (min,max) IP numbers
function set_dst_ip_range() {
    if [ -z "$2" ]; then
	echo "[$FUNCNAME] input error"
	exit 2
    fi
    local min=$1
    local max=$2
    echo "- Random IP destinations min:$min - max:$max"
    pgset "flag IPDST_RND"
    pgset "dst_min $min"
    pgset "dst_max $max"
}

# Setup flow generation
# Input (flows,flowlen)
function set_flows() {
    if [ -z "$2" ]; then
	echo "[$FUNCNAME] input error"
	exit 2
    fi
    local flows=$1
    local flowlen=$2
    echo "- Setup flow generation: flows:$flows flowlen:$flowlen "
    pgset "flag FLOW_SEQ"
    pgset "flows $flows"
    pgset "flowlen $flowlen"
}


# Input (min,max) port numbers
function set_udp_src_range() {
    if [ -z "$2" ]; then
	echo "[$FUNCNAME] input error"
	exit 2
    fi
    local min=$1
    local max=$2
    echo "- Random UDP source port min:$min - max:$max"
    pgset "flag UDPSRC_RND"
    pgset "udp_src_min $min"
    pgset "udp_src_max $max"
}

# Input (min,max) port numbers
function set_udp_dst_range() {
    if [ -z "$2" ]; then
	echo "[$FUNCNAME] input error"
	exit 2
    fi
    local min=$1
    local max=$2
    echo "- Random UDP destination port min:$min - max:$max"
    pgset "flag UDPDST_RND"
    pgset "udp_dst_min $min"
    pgset "udp_dst_max $max"
}

