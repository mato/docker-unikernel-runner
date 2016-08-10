.PHONY: build
build: mir-stackv4-ukvm mir-stackv4-qemu

.PHONY: clean clobber
clean:
	$(RM) mir-stackv4-ukvm.tar.gz
	$(RM) mir-stackv4-virtio.tar.gz

clobber: clean
	-docker rmi -f \
	    mir-stackv4-ukvm mir-stackv4-ukvm-build \
	    mir-stackv4-qemu mir-stackv4-virtio-build

.PHONY: run
run:
	./test-stackv4-ukvm.sh
	./test-stackv4-qemu.sh

# Mirage 'stackv4' sample (ukvm): intermediate build container.
mir-stackv4-ukvm.tar.gz: Dockerfile.stackv4-ukvm-build
	docker build -t mir-stackv4-ukvm-build -f Dockerfile.stackv4-ukvm-build .
	docker run --rm mir-stackv4-ukvm-build > mir-stackv4-ukvm.tar.gz

# Mirage 'stackv4' sample (virtio): intermediate build container.
mir-stackv4-virtio.tar.gz: Dockerfile.stackv4-virtio-build
	docker build -t mir-stackv4-virtio-build -f Dockerfile.stackv4-virtio-build .
	docker run --rm mir-stackv4-virtio-build > mir-stackv4-virtio.tar.gz

# Mirage 'stackv4' sample (ukvm): mir-stackv4-ukvm.
.PHONY: mir-stackv4-ukvm
mir-stackv4-ukvm: mir-stackv4-ukvm.tar.gz Dockerfile.stackv4-ukvm
	docker build -t mir-stackv4-ukvm -f Dockerfile.stackv4-ukvm .

# Mirage 'stackv4' sample (qemu): mir-stackv4-qemu.
.PHONY: mir-stackv4-qemu
mir-stackv4-qemu: mir-stackv4-virtio.tar.gz Dockerfile.stackv4-qemu
	docker build -t mir-stackv4-qemu -f Dockerfile.stackv4-qemu .
