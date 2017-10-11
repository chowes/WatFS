#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <iostream>
#include <memory>
#include <string>

#include <grpc++/grpc++.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/security/server_credentials.h>

#include "watfs.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using watfs::WatFS;
using watfs::WatFSStatus;
using watfs::WatFSGetAttrArgs;
using watfs::WatFSGetAttrRet;

class WatFSServer final : public WatFS::Service {
public:
    explicit WatFSServer(const char *root_dir) {
        // here we want to set up the server to use the specified root directory
    }

    Status WatFSNull(ServerContext *context, const WatFSStatus *client_status,
                     WatFSStatus *server_status) override {
        
        // just echo the status back to the client, like a heartbeat
        server_status->set_status(client_status->status());

        return Status::OK;
    }


    /*
     * The server fills in a struct stat buffer with information on whatever
     * file is indicated by the provided file path, and this struct stat is sent
     * back to the client. 
     *
     * In addition, we set an error code so the client can interpret errors
     */
    Status WatFSGetAttr(ServerContext *context, const WatFSGetAttrArgs *args,
                        WatFSGetAttrRet *attr) override {
        
        struct stat statbuf;
        std::string marshalled_data;
        
        int err;

        memset(&statbuf, 0, sizeof statbuf);
        err = stat(args->file_path().c_str(), &statbuf);
        marshalled_data.assign((const char*)&statbuf, sizeof statbuf);

        attr->set_attr(marshalled_data);
        if (err == -1) {
            // we want to set err so FUSE can throw informative errors
            attr->set_err(errno);
        } else {
            // no error, set err to 0
            attr->set_err(err);
        }

        return Status::OK;
    }
};



static void print_usage()
{
    std::cout << "usage: ./watfs_grpc_server [options] <rootdir> <address:port>"
              << std::endl;
}


void StartWatFSServer(const char *root_dir, const char *server_address)
{
    WatFSServer service(root_dir);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}


int main(int argc, const char *argv[])
{
    const char *root_dir;
    const char *server_address;

    // we don't have any options yet, just ignore them for now
    while (getopt(argc, (char **)argv, "abc:") != -1);
    
    root_dir = argv[optind++];
    if (root_dir == NULL) {
        print_usage();
        return 1;
    }

    server_address = argv[optind++];
    if (server_address == NULL) {
        print_usage();
        return 1;
    }

    std::cout << root_dir << std::endl;

    StartWatFSServer(root_dir, server_address);

    return 0;
}
