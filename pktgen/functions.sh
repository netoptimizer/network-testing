#
# Common functions used by pktgen scripts
#
# Author: Jesper Dangaaard Brouer
# License: GPL

if [ ! -d /proc/net/pktgen ]; then
        modprobe pktgen
fi

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
    if [ -n "$VERBOSE" ]; then
	echo "INFO : $@" >&2
    fi
}

## -- General shell tricks --

function root_check_run_with_sudo() {
    # Trick so, program can be run as normal user, will just use "sudo"
    #  call as root_check_run_as_sudo "$@"
    if [ "$EUID" -ne 0 ]; then
	if [ -x $0 ]; then # Directly executable use sudo
	    info "Not root, running with sudo"
            sudo "$0" "$@"
            exit $?
	fi
	err 4 "cannot perform sudo run of $0"
    fi
}

## -- Generic proc commands -- ##

function pgset() {
    local result

    if [ "$DEBUG" == "yes" ]; then
	echo "cmd: $1 > $PGDEV"
    fi
    echo $1 > $PGDEV
    local res=$?
    if [ $res -ne 0 ]; then
	warn "[$FUNCNAME] some error($res) occured cmd: $1 > $PGDEV"
    fi

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
	err 3 "proc file:$proc_ctrl does not exists!"
    else
	if [ ! -w "$proc_ctrl" ]; then
	    err 4 "proc file:$proc_ctrl not writable, not root?!"
	fi
    fi

    if [ "$DEBUG" == "yes" ]; then
	echo "cmd: $@ > $proc_ctrl"
    fi
    # Quoting of "$@" is important for space expansion
    echo "$@" > "$proc_ctrl"
    local status=$?

    # FIXME: Why "fgrep"
    result=`cat $proc_ctrl | fgrep "Result: OK:"`
    if [ "$result" = "" ]; then
	cat $proc_ctrl | fgrep Result: >&2
	if [ $status -ne 0 ]; then
	    err 5 "Write error($status) occured cmd: \"$@ > $proc_ctrl\""
	fi
    fi
}

function pg_thread() {
    local thread=$1
    local proc_file="kpktgend_${thread}"
    shift
    proc_cmd ${proc_file} "$@"
}

function pg_ctrl() {
    local proc_file="pgctrl"
    proc_cmd ${proc_file} "$@"
}

function pg_set() {
    local dev=$1
    local proc_file="$dev"
    shift
    proc_cmd ${proc_file} "$@"
}

## -- Pgcontrol commands -- ##

function start_run() {
    echo "Running... ctrl^C to stop"
    pg_ctrl "start"
    echo "Done"
}

function reset_all_threads() {
    info "Resetting all threads"
    # This might block if another start is running
    pg_ctrl "reset"
    info "Done - reset"
}

## -- Thread control commands -- ##

function remove_thread() {
    if [ -z "$1" ]; then
	echo "[$FUNCNAME] needs thread number"
	exit 2;
    fi
    local num="$1"

    echo "[$FUNCNAME] Removing all devices from thread $num"
    pg_thread $num "rem_device_all"
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
    if [ -z "$2" ]; then
	echo "[$FUNCNAME] needs args device + thread"
	exit 2;
    fi
    local thread="$2"
    local plain_device="$3"
    if [ -n "$plain_device" ]; then
	# Use if you don't want auto adding the @thread after dev name
	local dev="$1"
    else
	local dev="$1@$thread"
    fi

    echo "Adding ${dev} to thread:$thread"
    pg_thread $thread "add_device ${dev}"
}

function create_thread() {
    if [ -z "$1" ]; then
        err 2 "[$FUNCNAME] require thread num and device (defaults to $DEV)"
    fi
    local thread=$1
    local dev=$2
    if [ -z "$dev" ]; then
	info "thread $thread add_device defaults to device $DEV"
	dev=$DEV
    fi

    info "Removing all devices from thread:$thread"
    pg_thread $thread "rem_device_all"

    local mqdev=${dev}@${thread}
    info "Adding device:${mqdev} to thread:$thread"
    pg_thread $thread "add_device ${mqdev}"
}

function create_threads() {
    if [ -z "$2" ]; then
        err "[$FUNCNAME] require thread-range) min and max"
        exit 2
    fi
    local min=$1
    local max=$2
    local dev=$3
    if [ -z "$dev" ]; then
	dev=$DEV
    fi

    for num in `seq $min $max`; do
	create_thread ${num} ${dev}
    done
}

## -- Device commands -- ##

# Common config for a dev
function base_config() {
    if [ -n "$1" ]; then
        local dev="$1"
    else
	err 2 "[$FUNCNAME] need device input"
    fi

    info "Base config of $dev"
    pg_set $dev "flag QUEUE_MAP_CPU"
    pg_set $dev "count $COUNT"
    pg_set $dev "clone_skb $CLONE_SKB"
    pg_set $dev "pkt_size $PKT_SIZE"
    pg_set $dev "delay $DELAY"
    pg_set $dev "dst_mac ${DST_MAC}"
}

function dev_set_dst_ip() {
    if [ -n "$2" ]; then
	local dev="$1"
	local IP="$2"
	echo "- Dev:$dev Destination IP:$IP"
	cmd_dev $dev "dst $IP"
    else
	err 2 "[$FUNCNAME] input error"
    fi
}

function set_dst_ip() {
    if [ -n "$1" ]; then
        local IP="$1"
        echo "- Destination IP:$IP"
        pgset "dst $IP"
    else
        err 2 "[$FUNCNAME] input error"
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

# Input (min,max) IP numbers
function set_src_ip_range() {
    if [ -z "$2" ]; then
	echo "[$FUNCNAME] input error"
	exit 2
    fi
    local min=$1
    local max=$2
    echo "- Random IP source min:$min - max:$max"
    pgset "flag IPSRC_RND"
    pgset "src_min $min"
    pgset "src_max $max"
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

# General func for setting $dev $key $value
function dev_set_key_value() {
    if [ -n "$2" ]; then
	local dev="$1"
	local key="$2"
	local val="$3"
	echo "- Dev:$dev Set $key=$val"
	cmd_dev $dev "$key $val"
    else
	err 2 "[$FUNCNAME] input error"
    fi
}

# General func for setting $dev $key $value
function dev_set_flag() {
    if [ -n "$2" ]; then
	local dev="$1"
	local key=flag
	local val="$2"
	echo "- Dev:$dev Set $key $val"
	cmd_dev $dev "$key $val"
    else
	err 2 "[$FUNCNAME] input error"
    fi
}
