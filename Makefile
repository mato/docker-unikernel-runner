.PHONY: all
all: mir-runner tests

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

# Runner base image: mir-runner.
.PHONY: mir-runner
mir-runner: runner.tar.gz Dockerfile.runner
	docker build -t mir-runner -f Dockerfile.runner .

.PHONY: clean clobber
clean:
	$(RM) runner.tar.gz
	$(MAKE) -C tests/mirage-unix clean
	$(MAKE) -C tests/mirage-solo5 clean

# Run to clean all images, include intermediate containers.
clobber: clean
	-docker rmi -f mir-runner mir-runner-build
	$(MAKE) -C tests/mirage-unix clobber
	$(MAKE) -C tests/mirage-solo5 clobber
