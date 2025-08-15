.PHONY: build clean_build clean_screens clean server client

build: clean
	mkdir build && cd build && cmake .. && make
	cd build && find . -type f ! \( -name 'client' -o -name 'server' \) -delete
	cd build && find . -type d -empty -delete

clean_build:
	rm -rf build
	
clean_screens:
	rm -rf screenshots

clean: clean_build clean_screens

server:
	./build/server/server --port 65533

client:
	./build/client/client --srv 127.0.0.1:65533 --period 10
