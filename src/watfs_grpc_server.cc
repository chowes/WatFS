#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

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
using watfs::WatFSLookupArgs;
using watfs::WatFSLookupRet;


using namespace std;


static unordered_map<string, fstream> file_handle_map;


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
        string marshalled_attr;
        
        int err;

        memset(&statbuf, 0, sizeof statbuf);
        err = stat(args->file_path().c_str(), &statbuf);
        marshalled_attr.assign((const char*)&statbuf, sizeof statbuf);

        attr->set_attr(marshalled_attr);
        if (err == -1) {
            // we want to set err so FUSE can throw informative errors
            attr->set_err(errno);
        } else {
            // no error, set err to 0
            attr->set_err(err);
        }

        return Status::OK;
    }


    /*
     * The server fills in a struct stat buffer with information on whatever
     * object is indicated by the provided file path, as well as another stat
     * buffer for its containing directory. If the object is a file, it is 
     * opened and a mapping is added to the file handle map. The object and 
     * directory attributes are sent back to the client, along with a file
     * handle consisting of its server-based file pathare sent back to the 
     * client.
     *
     * In addition, we set an error code so the client can interpret errors,
     * but this doens't seem to be informative in all cases...
     */
    Status WatFSLookup(ServerContext *context, const WatFSLookupArgs *args,
                       WatFSLookupRet *ret) override {

        string file_path;
        string marshalled_file_attr;
        string marshalled_dir_attr;

        struct stat file_stat;
        struct stat dir_stat;

        int err;

        // concatenate the directory handle and file name to get a path
        file_path = args->dir_handle() + args->file_name();

        // use stat to get dir and file attributes
        memset(&dir_stat, 0, sizeof dir_stat);
        err = stat(args->dir_handle().c_str(), &dir_stat);
        marshalled_dir_attr.assign((const char*)&dir_stat, sizeof dir_stat);

        /* we "short circuit" here, make sure to check for a non-zero err,
         * since the rest of the return params won't be valid */
        if (err) {
            ret->set_err(errno);
            perror("stat - directory");
            return Status::OK;
        }
        ret->set_dir_attr(marshalled_dir_attr);

        memset(&file_stat, 0, sizeof file_stat);
        err = stat(file_path.c_str(), &file_stat);
        marshalled_file_attr.assign((const char*)&file_stat, sizeof file_stat);

        /* we "short circuit" here, make sure to check for a non-zero err,
         * since the rest of the return params won't be valid */
        if (err) {
            ret->set_err(errno);
            perror("stat - file");
            return Status::OK;
        }
        ret->set_file_attr(marshalled_file_attr);

        // if the object is a directory, we just want to return its path
        if (!S_ISREG(file_stat.st_mode)) {
            ret->set_file_handle(file_path);
            return Status::OK;
        }


        /* 
         * now we know the object is a file, so we add it to our mapping of
         * file paths to file handles, and return the file path to the client.
         * 
         * adding fstream objects to maps is kind of tricky. this code creates
         * a <string, fstream> pair, where the key is the file path and adds 
         * it to our map. then we pull this pair out of the map, asset that it
         * exists, and use it to open the file
         */

        // first check to see if the server has a handle for this file already
        auto file = file_handle_map.find(file_path);
        if (file == file_handle_map.end()) {

            /* we don't already have a copy of this file open, so we open it
             * and add the handle to the file handle map */
            file_handle_map.insert(make_pair(file_path, fstream{}));
            auto file = file_handle_map.find(file_path);
            assert(file != file_handle_map.end());
            

            // we don't know what we're reading, so assume in/out and binary mode
            file->second.open(file_path, ios::in | ios::out | ios::binary);
            if (file->second.fail()) {
                ret->set_err(errno);
                perror("open - file");
                file_handle_map.erase(file_path);
                return Status::OK;
            }

            /* now on subsequent reads/writes, we get our file handle from the
             * map using the file path the client uses as a handle*/
        }
        ret->set_file_handle(file_path);

        
        ret->set_err(0);

        return Status::OK;
    }
};



static void print_usage()
{
    cout << "usage: ./watfs_grpc_server [options] <rootdir> <address:port>"
         << endl;
}


void StartWatFSServer(const char *root_dir, const char *server_address)
{
    WatFSServer service(root_dir);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    unique_ptr<Server> server(builder.BuildAndStart());

    cout << "Server listening on " << server_address << endl;

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

    cout << "Server set mount directory to: " << root_dir << endl;
    StartWatFSServer(root_dir, server_address);

    return 0;
}