# Docker unikernel runner for Mirage OS [![Build status](https://travis-ci.org/mato/docker-unikernel-runner.svg?branch=master)](https://travis-ci.org/mato/docker-unikernel-runner)

This is an experimental unikernel runner for running
[Mirage OS](https://mirage.io) unikernels in Docker containers. Currently the
following Mirage OS targets are supported:

* `unix`: UNIX userspace using the `direct` network stack.
* `ukvm`: Mirage OS/[Solo5](https://github.com/solo5/solo5) using ukvm as the hypervisor.
* `qemu`, `kvm` (_experimental_): Mirage OS/[Solo5](https://github.com/solo5/solo5)
  using software emulation (`qemu`) or QEMU/KVM (`kvm`) as the hypervisor.

## Building

You will need `docker` (obviously) and `make` to drive the top-level build
process. The build itself is all run in containers so there are no other host
requirements.

To build the runner and all tests, run:

````
make
````

## Running the sample containers

Use `make run-tests` to run all tests available on your host. The Mirage/Solo5
tests require KVM and access to `/dev/kvm`.

### Mirage OS/unix

Two containers which build Mirage OS samples from the `mirage-skeleton`
repository are included, `mir-stackv4` and `mir-static_website`.

Each is run as a normal Docker container, however you must pass `/dev/net/tun`
to the container and run with the `CAP_NET_ADMIN` capability. For example:

````
docker run -ti --rm \
    --device=/dev/net/tun:/dev/net/tun \
    --cap-add=NET_ADMIN mir-stackv4
````
`CAP_NET_ADMIN` and access to `/dev/net/tun` are required for runner to be able
to wire L2 network connectivity from Docker to the unikernel. Runner will drop
all capabilities with the exception of `CAP_NET_BIND_SERVICE` before launching
the unikernel.

### Mirage OS/Solo5

To run the `mir-stackv4` sample using `ukvm` as a hypervisor:

````
docker run -ti --rm \
    --device=/dev/kvm:/dev/kvm \
    --device=/dev/net/tun:/dev/net/tun \
    --cap-add=NET_ADMIN mir-stackv4-ukvm
````
In addition to the requirements for the `unix` target, access to `/dev/kvm` is
required.

## Known issues

* ([mirage/mirage-net-unix#23](https://github.com/mirage/mirage-net-unix/issues/23)) Mirage OS `direct` networking stack on `unix` always starts with the same MAC address. Until this is fixed it is not possible to run more than one unikernel.
* ([#1](https://github.com/mato/docker-unikernel-runner/issues/1)) Network delays due to random MAC address use. 
* `qemu` and `kvm` support is experimental, currently uses Debian to build the containers due to unknown issues with the Alpine toolchain. Also, this setup currently uses a hard-coded MAC address, so you will not be able to run more than one unikernel.
