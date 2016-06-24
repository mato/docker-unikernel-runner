.PHONY: all
all: mir-runner mir-stackv4 mir-static_website

# Runner base image: intermediate build container.
runner.tar.gz: src/*.c src/Makefile Dockerfile.runner-build
	docker build -t mir-runner-build -f Dockerfile.runner-build .
	docker run --rm mir-runner-build > runner.tar.gz

# Runner base image: mir-runner.
.PHONY: mir-runner
mir-runner: runner.tar.gz Dockerfile.runner
	docker build -t mir-runner -f Dockerfile.runner .

# Mirage 'stackv4' sample: intermediate build container.
mir-stackv4.tar.gz: Dockerfile.stackv4-build
	docker build -t mir-stackv4-build -f Dockerfile.stackv4-build .
	docker run --rm mir-stackv4-build > mir-stackv4.tar.gz

# Mirage 'stackv4' sample: mir-stackv4.
.PHONY: mir-stackv4
mir-stackv4: mir-stackv4.tar.gz Dockerfile.stackv4 mir-runner
	docker build -t mir-stackv4 -f Dockerfile.stackv4 .

# Mirage 'static_website' sample: intermediate build container.
mir-static_website.tar.gz: Dockerfile.static_website-build
	docker build -t mir-static_website-build -f Dockerfile.static_website-build .
	docker run --rm mir-static_website-build > mir-static_website.tar.gz

# Mirage 'static_website' sample: mir-static_website.
.PHONY: mir-static_website
mir-static_website: mir-static_website.tar.gz Dockerfile.static_website mir-runner
	docker build -t mir-static_website -f Dockerfile.static_website .

.PHONY: clean clobber
clean:
	$(RM) runner.tar.gz mir-stackv4.tar.gz mir-static_website.tar.gz

# Run to clean all images, include intermediate containers.
clobber: clean
	docker rmi -f mir-runner mir-runner-build \
	    mir-stackv4 mir-stackv4-build \
	    mir-static_website mir-static_website-build
