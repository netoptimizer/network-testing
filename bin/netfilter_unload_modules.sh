#!/bin/bash
#
# Kills all iptables rules and unloads all iptables/netfilter related
# kernel modules.
#
# Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>

# Trick so, program can be run as normal user, will just use "sudo"
if [ "$EUID" -ne 0 ]; then
    # Directly executable easy
    if [ -x $0 ]; then
	sudo $0
	exit $?
    fi
    echo "ERROR: cannot perform sudo run of $0"
    exit 4
fi



iptables -F ; iptables -t nat -F; iptables -t mangle -F
iptables -X ; iptables -t nat -X; iptables -t mangle -X
iptables -t raw -F ; iptables -t raw -X

ip6tables -F ; ip6tables -t nat -F; ip6tables -t mangle -F
ip6tables -X ; ip6tables -t nat -X; ip6tables -t mangle -X
ip6tables -t raw -F ; ip6tables -t raw -X

ebtables -F           ; ebtables -X
ebtables -t nat -F    ; ebtables -t nat -X
ebtables -t broute -F ; ebtables -t broute -X

rmmod ebtable_filter ebtable_nat ebtable_broute ebtables
rmmod ip_set

rmmod ip6t_REJECT nf_reject_ipv6
rmmod ipt_REJECT  nf_reject_ipv4

rmmod ip6t_rpfilter

rmmod ipt_SYNPROXY nf_synproxy_core xt_CT \
      nf_nat_masquerade_ipv4 \
      nf_conntrack_ftp nf_conntrack_tftp nf_conntrack_irc nf_nat_tftp \
      ipt_MASQUERADE nf_MASQUERADE \
      iptable_nat nf_nat_ipv4 nf_conntrack_ipv4 nf_nat \
      nf_conntrack_ipv6 xt_state iptable_raw \
      iptable_filter iptable_raw iptable_mangle xt_CHECKSUM \
      ip_tables nf_defrag_ipv4 \
      xt_LOG xt_multiport \
      xt_tcpudp xt_conntrack

rmmod ip6table_mangle ip6table_raw ip6table_nat ip6table_filter ip6_tables

rmmod nf_nat_ipv6 nf_conntrack_ipv6 nf_defrag_ipv6

rmmod nf_nat nf_conntrack

rmmod x_tables

