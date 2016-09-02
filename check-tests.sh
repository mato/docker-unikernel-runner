#!/bin/sh

ARP_ACCEPT=$(sysctl -n net.ipv4.conf.docker0.arp_accept)
if [ "${ARP_ACCEPT}" != "1" ]; then
    cat <<EOM 1>&2
>>> WARNING: Run "sysctl -w net.ipv4.conf.docker0.arp_accept=1" to enable
>>> WARNING: gratuitous ARP on the Docker bridge, otherwise you will experience
>>> WARNING: intermittent network failures.
>>> WARNING: Now sleeping 10 seconds before running tests.
EOM
    sleep 10
fi
