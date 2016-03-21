# Set to '-q' to quiet docker build
Q=

.PHONY: all
all: unikernel-runner unikernel-mathopd unikernel-wopr

unikernel-runner.tar.gz: runner/*.go Dockerfile.runner-build
	docker build $(Q) -t unikernel-runner-build -f Dockerfile.runner-build .
	docker run --rm unikernel-runner-build > unikernel-runner.tar.gz

qemu.tar.gz: Dockerfile.qemu-build
	docker build $(Q) -t unikernel-qemu-build -f Dockerfile.qemu-build .
	docker run --rm unikernel-qemu-build > qemu.tar.gz

.PHONY: unikernel-runner
unikernel-runner: unikernel-runner.tar.gz qemu.tar.gz Dockerfile.runner
	docker build $(Q) -t unikernel-runner -f Dockerfile.runner .

unikernel-mathopd.tar.gz: Dockerfile.mathopd-build
	docker build $(Q) -t unikernel-mathopd-build -f Dockerfile.mathopd-build .
	docker run --rm unikernel-mathopd-build > unikernel-mathopd.tar.gz

.PHONY: unikernel-mathopd
unikernel-mathopd: unikernel-mathopd.tar.gz Dockerfile.mathopd config-mathopd.json
	docker build $(Q) -t unikernel-mathopd -f Dockerfile.mathopd .

unikernel-wopr.tar.gz: Dockerfile.wopr-build wopr/wopr.c
	docker build $(Q) -t unikernel-wopr-build -f Dockerfile.wopr-build .
	docker run --rm unikernel-wopr-build > unikernel-wopr.tar.gz

.PHONY: unikernel-wopr
unikernel-wopr: unikernel-wopr.tar.gz Dockerfile.wopr
	docker build $(Q) -t unikernel-wopr -f Dockerfile.wopr .

.PHONY: run-mathopd
run-mathopd:
	docker run --rm -ti \
	    --device /dev/kvm:/dev/kvm \
	    --device /dev/net/tun:/dev/net/tun \
	    --cap-add NET_ADMIN \
	    unikernel-mathopd

.PHONY: run-wopr
run-wopr:
	docker run --rm -ti \
	    --device /dev/kvm:/dev/kvm \
	    --device /dev/net/tun:/dev/net/tun \
	    --cap-add NET_ADMIN \
	    unikernel-wopr

.PHONY: clean
clean:
	-docker rmi -f unikernel-runner unikernel-runner-build
	-docker rmi -f unikernel-mathopd unikernel-mathopd-build
	-docker rmi -f unikernel-wopr unikernel-wopr-build
	rm -f unikernel-runner.tar.gz unikernel-mathopd.tar.gz
	rm -f unikernel-wopr.tar.gz

# QEMU takes ages to build, so don't clean it by default.
.PHONY: clean-qemu
clean-qemu:
	-docker rmi -f unikernel-qemu-build
	rm -f qemu.tar.gz

.PHONY: push
push: all
	docker tag unikernel-runner mato/unikernel-runner
	docker tag unikernel-wopr mato/unikernel-wopr
	docker tag unikernel-mathopd mato/unikernel-mathopd
	docker push mato/unikernel-runner
	docker push mato/unikernel-wopr
	docker push mato/unikernel-mathopd
