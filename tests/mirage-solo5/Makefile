.PHONY: build
build: mir-stackv4-ukvm

.PHONY: clean clobber
clean:
	$(RM) mir-stackv4-ukvm.tar.gz

clobber: clean
	-docker rmi -f \
	    mir-stackv4-ukvm mir-stackv4-ukvm-build

.PHONY: run
run:
	./test-stackv4-ukvm.sh

# Mirage 'stackv4' sample (ukvm): intermediate build container.
mir-stackv4-ukvm.tar.gz: Dockerfile.stackv4-ukvm-build
	docker build -t mir-stackv4-ukvm-build -f Dockerfile.stackv4-ukvm-build .
	docker run --rm mir-stackv4-ukvm-build > mir-stackv4-ukvm.tar.gz

# Mirage 'stackv4' sample (ukvm): mir-stackv4-ukvm.
.PHONY: mir-stackv4-ukvm
mir-stackv4-ukvm: mir-stackv4-ukvm.tar.gz Dockerfile.stackv4-ukvm
	docker build -t mir-stackv4-ukvm -f Dockerfile.stackv4-ukvm .
