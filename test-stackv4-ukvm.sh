#!/bin/sh
set -ex
docker run -d --name test-mir-stackv4-ukvm \
    --device=/dev/kvm:/dev/kvm \
    --device=/dev/net/tun:/dev/net/tun \
    --cap-add=NET_ADMIN mir-stackv4-ukvm
IP=$(docker inspect --format "{{ .NetworkSettings.IPAddress }}" test-mir-stackv4-ukvm)
echo -n Hello | nc ${IP} 8080
docker logs test-mir-stackv4-ukvm | tail -10
docker kill test-mir-stackv4-ukvm
docker rm test-mir-stackv4-ukvm

