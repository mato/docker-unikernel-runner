.PHONY: build
build: mir-stackv4 mir-static_website

.PHONY: clean clobber
clean:
	$(RM) mir-stackv4.tar.gz mir-static_website.tar.gz

clobber: clean
	-docker rmi -f \
	    mir-stackv4 mir-stackv4-build \
	    mir-static_website mir-static_website-build

.PHONY: run
run:
	./test-stackv4.sh
	./test-static_website.sh

# Mirage 'stackv4' sample: intermediate build container.
mir-stackv4.tar.gz: Dockerfile.stackv4-build
	docker build -t mir-stackv4-build -f Dockerfile.stackv4-build .
	docker run --rm mir-stackv4-build > mir-stackv4.tar.gz

# Mirage 'stackv4' sample: mir-stackv4.
.PHONY: mir-stackv4
mir-stackv4: mir-stackv4.tar.gz Dockerfile.stackv4
	docker build -t mir-stackv4 -f Dockerfile.stackv4 .

# Mirage 'static_website' sample: intermediate build container.
mir-static_website.tar.gz: Dockerfile.static_website-build
	docker build -t mir-static_website-build -f Dockerfile.static_website-build .
	docker run --rm mir-static_website-build > mir-static_website.tar.gz

# Mirage 'static_website' sample: mir-static_website.
.PHONY: mir-static_website
mir-static_website: mir-static_website.tar.gz Dockerfile.static_website
	docker build -t mir-static_website -f Dockerfile.static_website .

