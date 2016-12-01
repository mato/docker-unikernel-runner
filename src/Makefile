VENDOR=$(abspath .)/vendor

# -Wno-cpp needed to silence complaints in libnl3 headers on musl.
CFLAGS=-Wall -Wno-cpp -Werror -O2 -g -std=gnu99 -D_GNU_SOURCE
CFLAGS+=-I$(VENDOR)/install/usr/local/include
CFLAGS+=-I$(VENDOR)/install/usr/local/include/libnl3
LDLIBS+=-L$(VENDOR)/install/usr/local/lib
LDLIBS+=-lnl-route-3 -lnl-3 -lcap-ng -lm

.PHONY: all
all: runner

$(VENDOR)/libnl/stamp-build: $(VENDOR)/libnl/Makefile
	$(MAKE) -C $(VENDOR)/libnl
	$(MAKE) -C $(VENDOR)/libnl DESTDIR=$(VENDOR)/install install
	touch $@

$(VENDOR)/libnl/Makefile: $(VENDOR)/libnl/configure
	cd $(VENDOR)/libnl && CFLAGS=-Wno-cpp ./configure --disable-shared \
	    --disable-cli \
	    --disable-pthreads

$(VENDOR)/libnl/configure:
	mkdir -p $(VENDOR)/libnl
	tar -C $(VENDOR)/libnl --strip-components=1 \
	    -xzf $(VENDOR)/libnl-3.2.25.tar.gz

$(VENDOR)/libcap-ng/stamp-build: $(VENDOR)/libcap-ng/Makefile
	$(MAKE) -C $(VENDOR)/libcap-ng
	$(MAKE) -C $(VENDOR)/libcap-ng DESTDIR=$(VENDOR)/install install
	touch $@

$(VENDOR)/libcap-ng/Makefile: $(VENDOR)/libcap-ng/configure
	cd $(VENDOR)/libcap-ng && ./configure --disable-shared \
	    --without-python --without-python3

$(VENDOR)/libcap-ng/configure:
	mkdir -p $(VENDOR)/libcap-ng
	tar -C $(VENDOR)/libcap-ng --strip-components=1 \
	    -xzf $(VENDOR)/libcap-ng-0.7.8.tar.gz

runner.o: $(VENDOR)/libnl/stamp-build $(VENDOR)/libcap-ng/stamp-build

runner: runner.o ptrvec.o
	$(CC) $(CFLAGS) -static -o $@ runner.o ptrvec.o $(LDLIBS)

.PHONY: clean
clean:
	$(RM) runner runner.o ptrvec.o
	-$(MAKE) -C $(VENDOR)/libnl clean
	-$(MAKE) -C $(VENDOR)/libcap-ng clean
	$(RM) $(VENDOR)/libnl/stamp-build
	$(RM) $(VENDOR)/libcap-ng/stamp-build
	$(RM) -r $(VENDOR)/install

.PHONY: distclean
distclean: clean
	$(RM) -r $(VENDOR)/libnl/ $(VENDOR)/libcap-ng/
