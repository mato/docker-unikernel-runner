# mir-runner build container (debian version)
FROM debian:jessie
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -q -y \
        --no-install-recommends \
        bison \
        build-essential \
        flex \
        libcap-ng-dev \
        libnl-3-dev \
        libnl-route-3-dev \
        linux-libc-dev \
        pkg-config \
    && apt-get clean
ADD ./src /src/runner
WORKDIR /src/runner
RUN make
CMD tar -czf - runner
