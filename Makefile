all: watfs_client watfs_server

watfs_client: src/watfs_client.cc
	g++ -Wall src/watfs_client.cc `pkg-config fuse3 --cflags --libs` -o watfs_client

watfs_server: src/watfs_server.cc
	g++ -Wall src/watfs_server.cc `pkg-config fuse3 --cflags --libs` -o watfs_server

clean:
	rm -f *.o watfs_client watfs_server