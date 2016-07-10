.PHONY: all
all: mir-runner mir-runner-qemu tests

.PHONY: tests
tests: mir-runner
	$(MAKE) -C tests/mirage-unix
	$(MAKE) -C tests/mirage-solo5

.PHONY: run-tests
run-tests: tests
	$(MAKE) -C tests/mirage-unix run
	$(MAKE) -C tests/mirage-solo5 run

# Runner base image: intermediate build container.
runner.tar.gz: src/*.c src/Makefile Dockerfile.runner-build
	docker build -t mir-runner-build -f Dockerfile.runner-build .
	docker run --rm mir-runner-build > runner.tar.gz

# Runner base image for UKVM, UNIX: mir-runner.
.PHONY: mir-runner
mir-runner: runner.tar.gz Dockerfile.runner
	docker build -t mir-runner -f Dockerfile.runner .

# Runner base image: intermediate build container (Debian build).
runner-debian.tar.gz: src/*.c src/Makefile Dockerfile.runner-debian-build
	docker build -t mir-runner-debian-build -f Dockerfile.runner-debian-build .
	docker run --rm mir-runner-debian-build > runner-debian.tar.gz

# Runner base image for QEMU, QEMU/KVM: mir-runner-qemu
.PHONY: mir-runner-qemu
mir-runner-qemu: runner-debian.tar.gz Dockerfile.runner-qemu
	docker build -t mir-runner-qemu -f Dockerfile.runner-qemu .

.PHONY: clean clobber
clean:
	$(RM) runner.tar.gz runner-debian.tar.gz
	$(MAKE) -C tests/mirage-unix clean
	$(MAKE) -C tests/mirage-solo5 clean

# Run to clean all images, include intermediate containers.
clobber: clean
	-docker rmi -f mir-runner mir-runner-build \
	    mir-runner-qemu mir-runner-debian-build
	$(MAKE) -C tests/mirage-unix clobber
	$(MAKE) -C tests/mirage-solo5 clobber
