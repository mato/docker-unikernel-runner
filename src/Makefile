PKGCONFIG_DEPS=libnl-3.0 libnl-route-3.0 libcap-ng

CFLAGS=-Wall -g -std=gnu99
CFLAGS+=$(shell pkg-config --cflags $(PKGCONFIG_DEPS))
LDLIBS+=$(shell pkg-config --libs $(PKGCONFIG_DEPS))

all: runner

runner: runner.c

clean:
	$(RM) runner
