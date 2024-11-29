#!/bin/bash
#
# System wide collection of BPF maps and progs
#  - Plus contents of individual prog_array "tail-call" maps
#
# This output can be parsed by other tools to generate
#  - graph showing possible tail-calls interacting
#

export VERBOSE=yes

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
    if [[ -n "$VERBOSE" ]]; then
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
root_check_run_with_sudo "$@"

export bpftool=$(which bpftool)
if (( $? != 0 )); then
    err 9 "Cannot find cmdline tool 'bpftool'"
fi

export jq=$(which jq)
if (( $? != 0 )); then
    err 9 "Cannot find cmdline tool 'bpftool'"
fi

info "Collecting: BPF-progs"
$bpftool --json prog show > data-bpftool-prog.json

info "Collecting: BPF-maps"
$bpftool --json map show > data-bpftool-map.json

info "Finding map types: prog_array"
prog_array_ids=$($jq '.[] | select(.type == "prog_array") |.id' data-bpftool-map.json)
info "[-] IDs" $prog_array_ids

info "Collecting map contents from: prog_array"
for ID in $prog_array_ids ; do
    info "[+] Process ID:${ID}"
    $bpftool --json map dump id $ID > data-prog_array_map_${ID}_contents.json
done
