MAPNIK_PLUGINDIR = $(shell mapnik-config --input-plugins)
BUILDTYPE ?= Release

GYP_REVISION=3464008

all: libvtile

./deps/gyp:
	git clone https://chromium.googlesource.com/external/gyp.git ./deps/gyp && cd ./deps/gyp && git checkout $(GYP_REVISION)

build/Makefile: ./deps/gyp gyp/build.gyp test/*
	deps/gyp/gyp gyp/build.gyp --depth=. -DMAPNIK_PLUGINDIR=\"$(MAPNIK_PLUGINDIR)\" -Goutput_dir=. --generator-output=./build -f make

libvtile: build/Makefile Makefile
	@$(MAKE) -C build/ BUILDTYPE=$(BUILDTYPE) V=$(V)

test/geometry-test-data:
	git submodule update --init

test: libvtile test/geometry-test-data
	DYLD_LIBRARY_PATH=$(MVT_LIBRARY_PATH) ./build/$(BUILDTYPE)/tests

testpack:
	rm -f ./*tgz
	npm pack
	tar -ztvf *tgz
	rm -f ./*tgz

install:
	install -m 0755 -o root -g root -d $(DESTDIR)/usr/include/mapbox/mapnik-vector-tile
	install -m 0644 -o root -g root src/*.hpp $(DESTDIR)/usr/include/mapbox/mapnik-vector-tile
	install -m 0644 -o root -g root src/*.ipp $(DESTDIR)/usr/include/mapbox/mapnik-vector-tile

clean:
	rm -rf ./build

.PHONY: test


