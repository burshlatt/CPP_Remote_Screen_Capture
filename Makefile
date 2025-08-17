.PHONY: build docs clean_build clean_docs clean_screens clean server client

build: clean
	mkdir build && cd build && cmake .. && make
	cd build && find . -type f ! \( -name 'client' -o -name 'server' \) -delete
	cd build && find . -type d -empty -delete

docs:
	doxygen Doxyfile

clean_build:
	rm -rf build

clean_docs:
	rm -rf docs
	
clean_screens:
	rm -rf screenshots

clean: clean_build clean_docs clean_screens

server:
	./build/server/server --port 65533

client:
	./build/client/client --srv 127.0.0.1:65533 --period 10
