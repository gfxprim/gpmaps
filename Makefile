CFLAGS?=-W -Wall -Wextra -O2
CFLAGS+=$(shell gfxprim-config --cflags)
gpmaps: LDLIBS=-lm -lgfxprim $(shell gfxprim-config --libs-widgets) $(shell gfxprim-config --libs-loaders) -lgps -lproj
BIN=gpmaps
SOURCES=$(wildcard *.c)
DEP=$(SOURCES:.c=.dep)

all: $(BIN) $(DEP) pia

pia:
	make -C libpia/

gpmaps: gpmaps.o libpia/libpia.o xqx_map.o xqx_map_tmc.o xqx_pixmap.o \
       xqx_map_cache.o xqx.o xqx_view.o xqx_map_layer.o xqx_grid_layer.o \
       xqx_gps_layer.o xqx_projection.o xqx_gps.o xqx_waypoints.o \
       xqx_waypoints_layer.o

%.dep: %.c
	$(CC) $(CFLAGS) -M $< -o $@

install:
	install -D $(BIN) -t $(DESTDIR)/usr/bin/
	install -m 644 -D layout.json $(DESTDIR)/etc/gp_apps/$(BIN)/layout.json
	make -C libpia/ install

-include $(DEP)

clean:
	rm -f $(BIN) *.dep *.o libpia/libpia.o
	make -C libpia/ clean
