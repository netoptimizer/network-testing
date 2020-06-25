#
# Common functions used by scripts in this directory
#  - Depending on bash 3 (or higher) syntax
#
# Author: Jesper Dangaaard Brouer <netoptimizer@brouer.com>
# License: GPLv2

## -- sudo trick --
function root_check_run_with_sudo() {
    # Trick so, program can be run as normal user, will just use "sudo"
    #  call as root_check_run_as_sudo "$@"
    if [ "$EUID" -ne 0 ]; then
	if [ -x $0 ]; then # Directly executable use sudo
	    echo "# (Not root, running with sudo)" >&2
            sudo "$0" "$@"
            exit $?
	fi
	echo "cannot perform sudo run of $0"
	exit 1
    fi
}

## -- General shell logging cmds --
function err() {
    local exitcode=$1
    shift
    echo -e "ERROR: $@" >&2
    exit $exitcode
}

function warn() {
    echo -e "WARN : $@" >&2
}

function info() {
    if [[ -n "$VERBOSE" ]]; then
	echo "# $@"
    fi
}

nft_cmd=$(which nft)
if (( $? != 0 )); then
    err 9 "Cannot find cmdline tool 'nft' for nftables"
fi

## -- Wrapper call for nft --
function nft() {
    if [[ -n "$VERBOSE" ]]; then
	echo "$nft_cmd $@"
    fi
    if [[ -n "$DRYRUN" ]]; then
	return
    fi
    $su $nft_cmd "$@"
    local status=$?
    if (( $status != 0 )); then
	if [[ "$allow_fail" == "" ]]; then
	    echo "ERROR - Exec error($status) occurred cmd: \"$nft_cmd $@\""
	    exit 2
	fi
    fi
}
