# Docker unikernel runner for MirageOS

This is an experimental unikernel runner for running MirageOS unikernels in
Docker containers. Currently only the MirageOS `unix` backend is supported,
with the `direct` network stack.

## Building

You will need `docker` (obviously) and `make` to drive the top-level build
process. The build itself is all run in containers so there are no other host
requirements.

To build the runner and samples, run:

````
make
````

## Running the sample containers

Two containers which build MirageOS samples from the `mirage-skeleton`
repository are included, `mir-stackv4` and `mir-static_website`.

Each is run as a normal Docker container, however you must pass `/dev/net/tun`
to the container and run with the `CAP_NET_ADMIN` capability. For example:

````
docker run -ti --rm --device=/dev/net/tun:/dev/net/tun \
    --cap-add=NET_ADMIN mir-stackv4
````
`CAP_NET_ADMIN` and access to `/dev/net/tun` are required for runner to be able
to wire L2 network connectivity from Docker to the unikernel. Runner will drop
all capabilities with the exception of `CAP_NET_BIND_SERVICE` before launching
the unikernel.
