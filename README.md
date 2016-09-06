# Docker unikernel runner for Mirage OS [![Build status](https://travis-ci.org/mato/docker-unikernel-runner.svg?branch=master)](https://travis-ci.org/mato/docker-unikernel-runner)

This is an experimental unikernel runner for running
[Mirage OS](https://mirage.io) unikernels in Docker containers. Currently the
following Mirage OS targets are supported:

* `unix`: UNIX userspace using the `direct` network stack.
* `ukvm`: Mirage OS/[Solo5](https://github.com/solo5/solo5) using ukvm as the hypervisor.
* `qemu`, `kvm` (_experimental_): Mirage OS/[Solo5](https://github.com/solo5/solo5)
  using software emulation (`qemu`) or QEMU/KVM (`kvm`) as the hypervisor.

# Quick start with a Mirage application

You will need `docker` (obviously) and `make` to drive the top-level build
process. The build itself is all run in containers so there are no other host
requirements.

1. Clone this repository, run `make`. This will build the `mir-runner` and
   `mir-runner-qemu` base images.
2. Place `docker-mirage.sh` somewhere in your $PATH.
3. In the directory containing your built Mirage application, run
   `docker-mirage.sh build HYPERVISOR -t my-unikernel`, where _HYPERVISOR_ is
   one of the supported targets (see **note**).
4. Run the unikernel with `docker-mirage.sh run --rm -ti my-unikernel`.

**Note:** If you're using Docker for Mac or Docker for Windows, then you will
only be able to _run_ images built for the `qemu` HYPERVISOR locally.

# Detailed instructions

This section covers more about how runner works, including how to manually
build your own unikernel images without the `docker-mirage` wrapper script.

## Building


To build the runner and all example containers, run:

````
make tests
````

See the `Makefile`s under the `tests/` directory for an example of how to
manually build unikernel images.

## Running the example containers

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

* ([#1](https://github.com/mato/docker-unikernel-runner/issues/1)) Network delays due to random MAC address use. Workaround is: `sysctl -w net.ipv4.conf.docker0.arp_accept=1`.
* `qemu` and `kvm` support is experimental, currently uses Debian to build the containers due to unknown issues with the Alpine toolchain.
