PKGCONFIG_DEPS=libnl-3.0 libnl-route-3.0 libcap-ng

CFLAGS=-Wall -Werror -O2 -g -std=gnu99 -D_GNU_SOURCE
CFLAGS+=$(shell pkg-config --cflags $(PKGCONFIG_DEPS))
LDLIBS+=$(shell pkg-config --libs $(PKGCONFIG_DEPS))

all: runner

runner: runner.c ptrvec.c

clean:
	$(RM) runner
