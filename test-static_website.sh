#!/bin/sh
set -ex
docker run -d --name test-mir-www --device=/dev/net/tun:/dev/net/tun \
    --cap-add=NET_ADMIN mir-static_website
IP=$(docker inspect --format "{{ .NetworkSettings.IPAddress }}" test-mir-www)
curl -v http://${IP}:8080/
docker logs test-mir-www | tail -10
docker kill test-mir-www
docker rm test-mir-www

