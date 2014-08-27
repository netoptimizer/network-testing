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
    echo "[$(date +'%Y-%m-%dT%H:%M:%S%z')] ERROR: $@" >&2
    exit $exitcode
}

function warn() {
    echo "[$(date +'%Y-%m-%dT%H:%M:%S%z')] WARN : $@" >&2
}

function info() {
    echo "[$(date +'%Y-%m-%dT%H:%M:%S%z')] INFO : $@"
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

    # FIXME: Why "fgrep"
    result=`cat $proc_ctrl | fgrep "Result: OK:"`
    # FIXME: Use the shell $? exit code instead
    if [ "$result" = "" ]; then
	warn "failed pktgen cmd: $@ > $proc_ctrl"
        cat $proc_ctrl | fgrep Result: >&2
    fi
}

function cmd_thread() {
    local thread=$1
    local proc_file="kpktgend_${thread}"
    shift
    proc_cmd ${proc_file} "$@"
}

function cmd_pgctrl() {
    local proc_file="pgctrl"
    proc_cmd ${proc_file} "$@"
}

function cmd_dev() {
    local dev=$1
    #local thread=$2
    local proc_file="$dev"
    shift
    proc_cmd ${proc_file} "$@"
}

## -- Pgcontrol commands -- ##

function start_run() {
    info "Running... ctrl^C to stop"
    cmd_pgctrl "start"
    info "Done"
}

function reset_all_threads() {
    info "Resetting all threads"
    # This might block if another start is running
    cmd_pgctrl "reset"
    info "Done - reset"
}

## -- Thread control commands -- ##

function remove_thread() {
    if [ -z "$1" ]; then
	echo "[$FUNCNAME] needs thread number"
	exit 2;
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
    cmd_thread $thread "add_device ${dev}"
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
    cmd_thread $thread "rem_device_all"

    local mqdev=${dev}@${thread}
    info "Adding device:${mqdev} to thread:$thread"
    cmd_thread $thread "add_device ${mqdev}"
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
	warn "[$FUNCNAME] need device input"
    fi

    echo "Base config of $PGDEV"
    pgset "count $COUNT"
    pgset "clone_skb $CLONE_SKB"
    pgset "pkt_size $PKT_SIZE"
    pgset "delay $DELAY"
    pgset "flag QUEUE_MAP_CPU"
    pgset "dst_mac ${DST_MAC}"
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
