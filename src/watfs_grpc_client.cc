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

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using watfs::WatFS;

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
private:
    std::unique_ptr<WatFS::Stub> stub_;
};

int main(int argc, const char *argv[])
{
    WatFSClient client(grpc::CreateChannel("localhost:8080", 
                                           grpc::InsecureChannelCredentials()));


    std::cout << client.WatFSNull() << std::endl;

    return 0;
}
