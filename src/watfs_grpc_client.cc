#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <iostream>
#include <memory>
#include <string>

#include <grpc++/grpc++.h>
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include "watfs.grpc.pb.h"

using watfs::WatFS;
using watfs::WatFSStatus;
using watfs::WatFSGetAttrArgs;
using watfs::WatFSGetAttrRet;

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;


class WatFSClient {
public:
    WatFSClient(std::shared_ptr<Channel> channel)
            : stub_(WatFS::NewStub(channel)) {}


    /* 
     * call WatFSNull to ping the server
     *
     * return: response from server on success
     *         -1 on failure
     */
    bool WatFSNull() {
        ClientContext context;
        WatFSStatus client_status;
        WatFSStatus server_status;

        client_status.set_status(0);

        Status status = stub_->WatFSNull(&context, client_status, 
                                         &server_status);

        if (!status.ok() || server_status.status() != client_status.status()) {
            std::cerr << "WatFSNull: rpc failed." << std::endl;
            return false;
        }

        return true;
    }

    /*
     * get attribute information from the server. 
     *
     * fills in the given struct stat with values for relevant file
     * 
     * returns 0 on success, or the given errno on failure. this should be set
     * in the FUSE implementation to give meaningful errors to the user.
     */
    int WatFSGetAttr(const char *filename, struct stat *statbuf) {
        ClientContext context;
        WatFSGetAttrArgs getattr_args;
        WatFSGetAttrRet getattr_ret;

        std::string marshalled_data;


        getattr_args.set_file_path(filename);

        Status status = stub_->WatFSGetAttr(&context, getattr_args, 
                                            &getattr_ret);

        if (!status.ok()) {
            /* 
             * something went wrong, we should probably differentiate this from
             * file system errors somehow...
             */
            return -1;
        }

        marshalled_data = getattr_ret.attr();
        memset(statbuf, 0, sizeof(struct stat));
        memcpy(statbuf, marshalled_data.data(), sizeof(struct stat));
        
        // return the errno (or hopefully 0)
        // we could set errno here, but better to do it in FUSE
        return getattr_ret.err();
    }

private:
    std::unique_ptr<WatFS::Stub> stub_;
};


int main(int argc, const char *argv[])
{
    struct stat attr;
    struct stat attr2;

    // memset(&attr, 0, sizeof attr);
    stat(argv[1], &attr2);

    WatFSClient client(grpc::CreateChannel("localhost:8080", 
                                           grpc::InsecureChannelCredentials()));


    std::cout << client.WatFSNull() << std::endl;
    if (client.WatFSGetAttr(argv[1], &attr)) {
        perror("Client::WatFSNull");
    }

    std::cout << attr.st_size << std::endl;

    return 0;
}
