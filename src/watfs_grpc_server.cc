#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

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

using watfs::WatFS;
using watfs::WatFSStatus;
using watfs::WatFSGetAttrArgs;
using watfs::WatFSGetAttrRet;
using watfs::WatFSLookupArgs;
using watfs::WatFSLookupRet;
using watfs::WatFSReadArgs;
using watfs::WatFSReadRet;
using watfs::WatFSWriteArgs;
using watfs::WatFSWriteRet;
using watfs::WatFSReaddirArgs;
using watfs::WatFSReaddirRet;
using watfs::WatFSCreateArgs;
using watfs::WatFSCreateRet;
using watfs::WatFSUnlinkArgs;
using watfs::WatFSUnlinkRet;
using watfs::WatFSRenameArgs;
using watfs::WatFSRenameRet;
using watfs::WatFSMkdirArgs;
using watfs::WatFSMkdirRet;
using watfs::WatFSRmdirArgs;
using watfs::WatFSRmdirRet;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::Status;

using namespace std;

// This is the recommended message size for streams
#define MESSAGE_SZ          16384

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
            perror("stat");
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
            perror("stat");
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
                perror("open");
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


    /*
     * The server reads from the file given by the WatFS file handle and fills 
     * in a struct stat buffer with information on whatever file is indicated by
     * the provided file handle. In addition, a boolean indicating the end of 
     * file, and the number of bytes read are sent back to the client.
     *
     * In addition, we set an error code so the client can interpret errors
     */
    Status WatFSRead(ServerContext *context, const WatFSReadArgs *args,
                        ServerWriter<WatFSReadRet> *writer) override {
        
        struct stat attr;
        int count;
        bool eof;

        string marshalled_attr;
        string marshalled_data;

        WatFSReadRet ret;

        int bytes_sent = 0;
        int err;

        auto file = file_handle_map.find(args->file_handle());
        if (file == file_handle_map.end()) {
            /* not open for reading, this isn't an error on the server side,
             * so we won't set errno */
            ret.set_err(EBADF);
            writer->Write(ret);
            return Status::OK;
        }
        
        memset(&attr, 0, sizeof attr);
        err = stat(args->file_handle().c_str(), &attr);
        if (file == file_handle_map.end()) {
            // not open for reading
            ret.set_err(errno);
            perror("stat");
            writer->Write(ret);
            return Status::OK;
        }

        marshalled_attr.assign((const char*)&attr, sizeof attr);
        ret.set_file_attr(marshalled_attr);

        // we still want to read data as a big chunk on the server
        char *data = new char[args->count()];

        // read count bytes from file start at offset
        file->second.clear();
        file->second.seekg(args->offset(), file->second.beg);

        file->second.read(data, args->count());
        count = file->second.gcount();
        eof = file->second.eof();
        if (file->second.bad()) {
            // not open for reading
            ret.set_err(errno);
            perror("read");
            writer->Write(ret);
            return Status::OK;
        }

        // no errors reading the file
        ret.set_err(0);

        int msg_sz; // the size of the message sent over the stream
        while (bytes_sent < args->count()) {
            // we want to send at most MESSAGE_SZ bytes at a time
            msg_sz = min(MESSAGE_SZ, args->count() - bytes_sent);
            // send this chunk over the stream
            
            marshalled_data.assign(data+bytes_sent, msg_sz);            
            ret.set_data(marshalled_data);
            ret.set_count(msg_sz);
            writer->Write(ret);

            bytes_sent += msg_sz;
        }

        return Status::OK;
    }


    /*
     * 
     */
    Status WatFSWrite(ServerContext *context, ServerReader<WatFSWriteArgs> 
                      *reader, WatFSWriteRet *ret) override {

        WatFSWriteArgs args;

        struct stat attr;

        string marshalled_attr;
        string marshalled_data;

        // bytes read from the stream
        int bytes_read = 0;
        int err;


        // we have to do our first read outside of the loop
        reader->Read(&args);

        auto file = file_handle_map.find(args.file_handle());
        if (file == file_handle_map.end()) {
            /* not open for reading, this isn't an error on the server side,
             * so we won't set errno */
            ret->set_err(EBADF);
            return Status::OK;
        }

        do {
            marshalled_data = args.data();

            // write to the proper offset
            file->second.clear();
            // note: client updates the offset for us on each iteration!
            file->second.seekp(args.offset());
            file->second.write(marshalled_data.data(), args.count());

            if (file->second.bad()) {
                ret->set_commit(false);
                ret->set_count(-1);
                ret->set_err(errno);
                perror("write");
                return Status::OK;
            }

            bytes_read += args.count();
        } while (reader->Read(&args));

         // always flush the buffer, not the same as sync
        file->second.flush();
        // this shouldn't really fail, but we'll be cautious
        if (file->second.bad()) {
            ret->set_commit(false);
            ret->set_count(-1);
            ret->set_err(errno);
            perror("flush");
            return Status::OK;
        }

        // set the total bytes written to file
        ret->set_count(bytes_read);

        if (args.commit()) {
            sync(); // syncs all changes, not just this file
            ret->set_commit(true);
        } else {
            ret->set_commit(false);
        }

        marshalled_attr.assign((const char*)&attr, sizeof attr);
        ret->set_file_attr(marshalled_attr);

        ret->set_err(0);

        return Status::OK;
    }


    /*
     * 
     */
    Status WatFSReaddir(ServerContext *context, const WatFSReaddirArgs *args,
                       ServerWriter<WatFSReaddirRet> *writer) override {

        DIR *dh;
        struct stat attr;
        struct dirent *dir_entry;
        
        string marshalled_attr;
        string marshalled_dir_entry;

        WatFSReaddirRet ret;


        dh = opendir(args->file_handle().c_str());
        dir_entry = readdir(dh);
        if (dir_entry == NULL) {
            // should never happen!
            ret.set_err(errno);
            writer->Write(ret);
            return Status::OK;
        }

        do {
            marshalled_dir_entry.assign((const char *)dir_entry, 
                                        sizeof(struct dirent));            
            ret.set_dir_entry(marshalled_dir_entry);
            
            memset(&attr, 0, sizeof attr);
            stat(args->file_handle().c_str(), &attr);
            marshalled_attr.assign((const char *)&attr, sizeof attr);
            ret.set_attr(marshalled_attr);

            writer->Write(ret);
        } while (dir_entry = readdir(dh));

        closedir(dh);

        return Status::OK;
    }


    /*
     *
     */
    Status WatFSCreate(ServerContext *context, const WatFSCreateArgs *args,
                       WatFSCreateRet *ret) {
        
    }


    /*
     *
     */
    Status WatFSUnlink(ServerContext *context, const WatFSUnlinkArgs *args,
                       WatFSLookupRet *ret) {
        
    }


    /*
     *
     */
    Status WatFSMkdir(ServerContext *context, const WatFSMkdirArgs *args,
                       WatFSMkdirRet *ret) {

    }


    /*
     *
     */
    Status WatFSRmdir(ServerContext *context, const WatFSRmdirArgs *args,
                       WatFSRmdirRet *ret) {
        
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
