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
using watfs::WatFSLookupArgs;
using watfs::WatFSLookupRet;

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;

using namespace std;


class WatFSClient {
public:
    WatFSClient(shared_ptr<Channel> channel)
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
            cerr << "WatFSNull: rpc failed." << endl;
            return false;
        }

        return true;
    }

    /*
     * get attribute information from the server. 
     *
     * fills in the given struct stat with values for relevant file
     * 
     * returns 0 on success, or -1 if the gRPC call failes. If the return value
     * is greater than 0, it indicates the errno indicated on the server. this
     * should be set in the FUSE implementation to give meaningful errors to 
     * the user.
     */
    int WatFSGetAttr(const char *filename, struct stat *statbuf) {
        ClientContext context;
        WatFSGetAttrArgs getattr_args;
        WatFSGetAttrRet getattr_ret;

        string marshalled_attr;


        getattr_args.set_file_path(filename);

        Status status = stub_->WatFSGetAttr(&context, getattr_args, 
                                            &getattr_ret);

        if (!status.ok()) {
            /* 
             * something went wrong, we should probably differentiate this from
             * file system errors somehow...
             */
            cerr << "WatFSGetAttr: rpc failed." << endl;
            return -1;
        }

        marshalled_attr = getattr_ret.attr();
        memset(statbuf, 0, sizeof(struct stat));
        memcpy(statbuf, marshalled_attr.data(), sizeof(struct stat));
        
        // return the errno (or hopefully 0)
        // we could set errno here, but better to do it in FUSE
        return getattr_ret.err();
    }


    /*
     * get a file handle from the server. 
     *
     * given the handle of a directory to search and a file name, returns a
     * file handle (the server representation of the file path), and fills in
     * a struct stat with file attributes, and another struct stat with
     * attributes for the containing directory 
     * 
     * returns 0 on success, or -1 if the gRPC call failes. If the return value
     * is greater than 0, it indicates the errno indicated on the server. this
     * should be set in the FUSE implementation to give meaningful errors to 
     * the user.
     */
    int WatFSLookup(const char *dir, const char *file, string &file_handle,
                    struct stat *file_stat, struct stat *dir_stat) {

        ClientContext context;
        WatFSLookupArgs lookup_args;
        WatFSLookupRet lookup_ret;

        string marshalled_file_handle;
        string marshalled_file_attr;
        string marshalled_dir_attr;


        lookup_args.set_dir_handle(dir);
        lookup_args.set_file_name(file);

        Status status = stub_->WatFSLookup(&context, lookup_args, &lookup_ret);

        if (!status.ok()) {
            /* 
             * something went wrong, we should probably differentiate this from
             * file system errors somehow...
             */
            cerr << "WatFSLookup: rpc failed." << endl;
            return -1;
        }


        // fill in our file handle and attribute structures
        marshalled_file_handle = lookup_ret.file_handle();
        marshalled_dir_attr = lookup_ret.dir_attr();
        marshalled_file_attr = lookup_ret.file_attr();

        file_handle = marshalled_file_handle;

        memset(dir_stat, 0, sizeof(struct stat));
        memcpy(dir_stat, marshalled_dir_attr.data(), sizeof(struct stat));
        
        memset(file_stat, 0, sizeof(struct stat));
        memcpy(file_stat, marshalled_file_attr.data(), sizeof(struct stat));

        // return the errno (or hopefully 0)
        // we could set errno here, but better to do it in FUSE
        return lookup_ret.err();
    }


private:
    unique_ptr<WatFS::Stub> stub_;
};


int main(int argc, const char *argv[])
{
    struct stat file_attr;
    struct stat dir_attr;
    string path;
    int err;

    WatFSClient client(grpc::CreateChannel("localhost:8080", 
                                           grpc::InsecureChannelCredentials()));

    if (err = client.WatFSGetAttr(argv[1], &file_attr)) {
        errno = err;
        perror("Client::WatFSGetAttr");
    } else {
        cout << file_attr.st_size << endl;
    }


    if (err = client.WatFSLookup(argv[1], argv[2], path, &file_attr, &dir_attr)) {
        errno = err;        
        perror("Client::WatFSLookup");
    } else {
        cout << path << endl;
    }

    return 0;
}
