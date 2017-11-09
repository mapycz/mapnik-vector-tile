GYP_REVISION=3464008

default: build/Makefile

pre_build_check:
	@echo "Looking for mapnik-config on your PATH..."
	mapnik-config -v

./deps/gyp:
	git clone https://chromium.googlesource.com/external/gyp.git ./deps/gyp && cd ./deps/gyp && git checkout $(GYP_REVISION)

build/Makefile: pre_build_check ./deps/gyp gyp/build.gyp test/*
	deps/gyp/gyp gyp/build.gyp --depth=. -DMAPNIK_PLUGINDIR=\"$(shell mapnik-config --input-plugins)\" -Goutput_dir=. --generator-output=./build -f make
	$(MAKE) -C build/ V=$(V)

test/geometry-test-data/README.md:
	git submodule update --init

test: test/geometry-test-data/README.md
	BUILDTYPE=Release ./test/run.sh

testpack:
	rm -f ./*tgz
	npm pack
	tar -ztvf *tgz
	rm -f ./*tgz

install:
	install -m 0755 -o root -g root -d $(DESTDIR)/usr/include/mapbox/mapnik-vector-tile
	install -m 0644 -o root -g root src/*.hpp $(DESTDIR)/usr/include/mapbox/mapnik-vector-tile
	install -m 0644 -o root -g root src/*.ipp $(DESTDIR)/usr/include/mapbox/mapnik-vector-tile
	install -m 0644 -o root -g root src/*.ipp $(DESTDIR)/usr/include/mapbox/mapnik-vector-tile
	install -m 0755 -o root -g root -d $(DESTDIR)/usr/src/mapbox/mapnik-vector-tile
	install -m 0644 -o root -g root build/Release/obj/gen/vector_tile.pb.* $(DESTDIR)/usr/src/mapbox/mapnik-vector-tile
	install -m 0755 -o root -g root -d $(DESTDIR)/usr/bin
	install -m 0755 -o root -g root build/Release/tileinfo $(DESTDIR)/usr/bin
	install -m 0755 -o root -g root build/Release/vtile-decode $(DESTDIR)/usr/bin
	install -m 0755 -o root -g root build/Release/vtile-edit $(DESTDIR)/usr/bin
	install -m 0755 -o root -g root build/Release/vtile-encode $(DESTDIR)/usr/bin

clean:
	rm -rf ./build

.PHONY: test build/Makefile


