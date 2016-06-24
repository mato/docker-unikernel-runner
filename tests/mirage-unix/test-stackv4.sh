#!/bin/sh
set -ex
docker run -d --name test-mir-stackv4 --device=/dev/net/tun:/dev/net/tun \
    --cap-add=NET_ADMIN mir-stackv4
IP=$(docker inspect --format "{{ .NetworkSettings.IPAddress }}" test-mir-stackv4)
echo -n Hello | nc ${IP} 8080
docker logs test-mir-stackv4 | tail -10
docker kill test-mir-stackv4
docker rm test-mir-stackv4

