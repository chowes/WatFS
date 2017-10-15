CXX = g++
CPPFLAGS += `pkg-config --cflags protobuf grpc fuse3`
CXXFLAGS += -std=c++11 -I. -Iinclude -g
LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf grpc++ fuse3`

PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

PROTOS_PATH = proto
SRC_PATH = src


vpath %.proto $(PROTOS_PATH)
vpath %.cc $(SRC_PATH)


all: watfs_server watfs_grpc_server client_test watfs_client

watfs_client: watfs_client.o watfs.pb.o watfs.grpc.pb.o watfs_grpc_client.o
	$(CXX) $^  $(LDFLAGS) -o $@

watfs_grpc_server: watfs.pb.o watfs.grpc.pb.o watfs_grpc_server.o
	$(CXX) $^  $(LDFLAGS) -o $@

watfs_grpc_client: watfs.pb.o watfs.grpc.pb.o watfs_grpc_client.o
	$(CXX) $^  $(LDFLAGS) -o $@

client_test: client_test.o watfs.pb.o watfs.grpc.pb.o watfs_grpc_client.o
	$(CXX) $^  $(LDFLAGS) -o $@


%.grpc.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

%.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --cpp_out=. $<

clean:
	rm -f *.o *.pb.cc *.pb.h watfs_client watfs_server watfs_grpc_server client_test
