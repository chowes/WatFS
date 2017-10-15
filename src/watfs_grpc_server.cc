#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
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


class WatFSServer final : public WatFS::Service {
public:
    explicit WatFSServer(const char *root_dir) {
        // here we want to set up the server to use the specified root directory
        root_directory.assign(root_dir);
        if (root_directory.back() != '/') {
            root_directory += "/";
        }
        cout << "WatFS server root directory set to: " + root_directory << endl;
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
        fstream fh;

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

        fh.open(file_path, ios::in | ios::out | ios::binary);
        if (fh.fail()) {
            ret->set_err(errno);
            perror("open");
            fh.close();
            return Status::OK;
        }
        fh.close();
        
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

        fstream fh;

        fh.open(args->file_handle(), ios::in | ios::binary);
        if (fh.fail()) {
            ret.set_err(errno);
            writer->Write(ret);
            perror("open");
            fh.close();
            return Status::OK;
        }
        

        memset(&attr, 0, sizeof attr);
        err = stat(args->file_handle().c_str(), &attr);
        if (err == -1) {
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
        fh.clear();
        fh.seekg(args->offset(), fh.beg);

        fh.read(data, args->count());
        count = fh.gcount();
        eof = fh.eof();
        if (fh.bad()) {
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

        fh.close();

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

        fstream fh;

        // we have to do our first read outside of the loop
        reader->Read(&args);

        fh.open(args.file_handle(), ios::out | ios::binary);
        if (fh.fail()) {
            ret->set_err(errno);
            perror("open");
            fh.close();
            return Status::OK;
        }

        do {
            marshalled_data = args.data();

            // write to the proper offset
            fh.clear();
            // note: client updates the offset for us on each iteration!
            fh.seekp(args.offset());
            fh.write(marshalled_data.data(), args.count());

            if (fh.bad()) {
                ret->set_commit(false);
                ret->set_count(-1);
                ret->set_err(errno);
                perror("write");
                return Status::OK;
            }

            bytes_read += args.count();
        } while (reader->Read(&args));

         // always flush the buffer, not the same as sync
        fh.flush();
        // this shouldn't really fail, but we'll be cautious
        if (fh.bad()) {
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

        fh.close();

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

        cout << args->file_handle() << endl;

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

        int err = creat(args->path().c_str(), args->mode());

        if (err == -1) {
            ret->set_err(errno);
            perror("creat");
        } else {
            ret->set_err(0);
        }

        close(err);
        return Status::OK;
    }


    /*
     *
     */
    Status WatFSUnlink(ServerContext *context, const WatFSUnlinkArgs *args,
                       WatFSUnlinkRet *ret) {
        int err = unlink(args->path().c_str());

        if (err == -1) {
            ret->set_err(errno);
            perror("unlink");
        } else {
            ret->set_err(0);
        }

        return Status::OK;
    }


    /*
     *
     */
    Status WatFRename(ServerContext *context, const WatFSRenameArgs *args,
                       WatFSRenameRet *ret) {
        int err = rename(args->source().c_str(), args->dest().c_str());

        if (err == -1) {
            ret->set_err(errno);
            perror("rename");
        } else {
            ret->set_err(0);
        }

        return Status::OK;
    }


    /*
     *
     */
    Status WatFSMkdir(ServerContext *context, const WatFSMkdirArgs *args,
                       WatFSMkdirRet *ret) {

        int err = mkdir(args->path().c_str(), args->mode());

        if (err == -1) {
            ret->set_err(errno);
            perror("rmdir");
        } else {
            ret->set_err(0);
        }

        return Status::OK;
    }


    /*
     *
     */
    Status WatFSRmdir(ServerContext *context, const WatFSRmdirArgs *args,
                       WatFSRmdirRet *ret) {
        int err = rmdir(args->path().c_str());

        if (err == -1) {
            ret->set_err(errno);
            perror("unlink");
        } else {
            ret->set_err(0);
        }

        return Status::OK;
    }


private:
    string root_directory;

    string translatePathname(string &pathname) {
        return root_directory + pathname;
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

    StartWatFSServer(root_dir, server_address);

    return 0;
}
