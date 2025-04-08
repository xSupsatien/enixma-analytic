PROG1	= enixma_analytic
OBJS1	= $(PROG1).c argparse.c imgprovider.c imgutils.c overlay.c detection.c deepsort.c roi.c counting.c fastcgi.c incident.c imwrite.c event.c
PROGS	= $(PROG1)
LIBDIR = lib
LIBJPEG_TURBO = /opt/build/libjpeg-turbo/build
URIPARSER = /opt/build/uriparser/build

PKGS = gio-2.0 gio-unix-2.0 liblarod vdostream glib-2.0 cairo fcgi jansson axevent

CFLAGS += -I$(LIBJPEG_TURBO)/include -I$(URIPARSER)/include -DLAROD_API_VERSION_3
LDLIBS += -ljpeg -luriparser
LDFLAGS += -L./$(LIBDIR) -Wl,-rpath,'$$ORIGIN/$(LIBDIR)'

CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))
LDFLAGS += -s -laxoverlay

CFLAGS += -Wall \
          -Wextra \
          -Wformat=2 \
          -Wpointer-arith \
          -Wbad-function-cast \
          -Wstrict-prototypes \
          -Wmissing-prototypes \
          -Winline \
          -Wdisabled-optimization \
          -Wfloat-equal \
          -W \
          -Werror

all:	$(PROGS)

$(PROG1): $(OBJS1)
	mkdir -p $(LIBDIR)
	cp $(LIBJPEG_TURBO)/lib/*.so* $(LIBDIR)/
	cp $(URIPARSER)/lib/*.so* $(LIBDIR)/
	$(CC) $^ $(CFLAGS) $(LIBS) $(LDFLAGS) -lm $(LDLIBS) -o $@
	$(STRIP) $@

clean:
	rm -rf $(PROGS) *.o $(LIBDIR) *.eap* *_LICENSE.txt manifest.json package.conf* param.conf tmp*