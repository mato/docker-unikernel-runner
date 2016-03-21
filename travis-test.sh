#!/bin/sh
# This script runs a full end-to-end test of docker-unikernel-runner:
#   - starts a mathopd unikernel
#   - verifies the web server is running and responding to requests
#   - kills the unikernel
# As the script is intended to run under Travis where KVM is not available,
# software emulation is used.
set -e
CID=$(docker run --detach \
    --device /dev/net/tun:/dev/net/tun \
    --cap-add NET_ADMIN \
    --publish=80 \
    unikernel-mathopd)
CPORT=$(docker port $CID 80)
echo Started ${CID}, listening on ${CPORT}
# Use wget in preference to curl as it will retry on "Connection reset by peer"
# which is what we get from Docker's port forwarding if networking (or mathopd)
# is not up yet.
wget -q -O - http://${CPORT}/
docker rm -f $CID
