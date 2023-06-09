TARGET:=$(shell gcc -dumpmachine)
BIN:=jerry

default: build/$(TARGET)/$(BIN)

build/$(TARGET)/Makefile: package.ini arch/$(TARGET) build/$(TARGET)/src build/$(TARGET)/lib
	mkdir -p build/$(TARGET)
	cd arch && tar -c $(TARGET) | tar -x -C ../build/$(TARGET)/ --strip-components=1

build/$(TARGET)/src: $(wildcard src/*.c)
	rm -rf build/$(TARGET)
	mkdir -p build/$(TARGET)
	cp -r src build/$(TARGET)/

build/$(TARGET)/lib:
	mkdir -p build/$(TARGET)
	cp package.ini build/$(TARGET)
	cd $(shell dirname $@) && dep install

build/$(TARGET)/$(BIN): build/$(TARGET)/Makefile build/$(TARGET)/src
	cd $(shell dirname $@) && $(MAKE) $(BIN) "TARGET=$(TARGET)" "BIN=$(BIN)"

.PHONY: clean
clean:
	rm -rf build
