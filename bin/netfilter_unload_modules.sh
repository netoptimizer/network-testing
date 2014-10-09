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

iptables -F ; iptables -t nat -F; iptables -t mangle -F ; ip6tables -F
iptables -X ; iptables -t nat -X; iptables -t mangle -X ; ip6tables -X
iptables -t raw -F ; iptables -t raw -X

rmmod ebtable_nat  ebtables
rmmod ipt_SYNPROXY nf_synproxy_core xt_CT \
      nf_nat_masquerade_ipv4 \
      nf_conntrack_ftp nf_conntrack_tftp nf_conntrack_irc nf_nat_tftp \
      ipt_MASQUERADE nf_MASQUERADE \
      iptable_nat nf_nat_ipv4 nf_nat nf_conntrack_ipv4 nf_nat \
      nf_conntrack_ipv6 xt_state nf_conntrack iptable_raw \
      nf_conntrack \
      iptable_filter iptable_raw iptable_mangle ipt_REJECT xt_CHECKSUM \
      ip_tables nf_defrag_ipv4 \
      ip6table_filter ip6_tables nf_defrag_ipv6 ip6t_REJECT \
      xt_LOG xt_multiport \
      xt_tcpudp xt_conntrack x_tables
