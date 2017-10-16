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
using watfs::WatFSTruncateArgs;
using watfs::WatFSTruncateRet;
using watfs::WatFSReaddirArgs;
using watfs::WatFSReaddirRet;
using watfs::WatFSMknodArgs;
using watfs::WatFSMknodRet;
using watfs::WatFSUnlinkArgs;
using watfs::WatFSUnlinkRet;
using watfs::WatFSRenameArgs;
using watfs::WatFSRenameRet;
using watfs::WatFSMkdirArgs;
using watfs::WatFSMkdirRet;
using watfs::WatFSRmdirArgs;
using watfs::WatFSRmdirRet;
using watfs::WatFSUtimensArgs;
using watfs::WatFSUtimensRet;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::Status;

using namespace std;

#define MESSAGE_SZ          8192


class WatFSServer final : public WatFS::Service {
public:
    explicit WatFSServer(const char *root_dir) {
        // here we want to set up the server to use the specified root directory
        root_directory.assign(root_dir);
        if (root_directory.back() == '/') {
            root_directory.erase(root_directory.end()-1);  
        }
        cout << "WatFS server root directory set to: " + root_directory << endl;
    }

    Status WatFSNull(ServerContext *context, const WatFSStatus *client_status,
                     WatFSStatus *server_status) override {
        
        // just echo the status back to the client, like a heartbeat
        server_status->set_status(client_status->status());

        cerr << "DEBUG: received ping from client" << endl;

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
        
        string file_path;
        struct stat statbuf;
        string marshalled_attr;
        
        int err;

        file_path = translate_pathname(args->file_path());

        memset(&statbuf, 0, sizeof statbuf);
        err = stat(file_path.c_str(), &statbuf);
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
        struct stat statbuf;
        fstream fh;

        int err;

        // concatenate the directory handle and file name to get a path
        file_path = translate_pathname(args->file_path());

        err = stat(file_path.c_str(), &statbuf);
        
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

        string path;
        string marshalled_data;

        WatFSReadRet ret;

        int bytes_sent = 0;
        int err;

        fstream fh;

        path = translate_pathname(args->file_handle());

        fh.open(path, ios::in | ios::binary);
        if (fh.fail()) {
            ret.set_err(errno);
            writer->Write(ret);
            perror("open");
            fh.close();
            return Status::OK;
        }


        // we want to read data as a big chunk on the server
        char *data = new char[args->count()];

        // read count bytes from file start at offset
        fh.clear();
        fh.seekg(args->offset(), fh.beg);

        fh.read(data, args->count());
        count = fh.gcount();
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

        delete data;
        fh.close();


        return Status::OK;
    }


    /*
     * 
     */
    Status WatFSWrite(ServerContext *context, ServerReader<WatFSWriteArgs> *reader, 
                      WatFSWriteRet *ret) override {
        
        WatFSWriteArgs args;
        string path;
        char *buffer;
        int bytes_recv = 0;
        int bytes_written = 0;

        int fd;

        reader->Read(&args);

        buffer = new char[args.total_size()];

        do {
            memcpy(buffer+bytes_recv, args.buffer().data(), args.size());
            bytes_recv += args.size();
        } while (reader->Read(&args));


        path = translate_pathname(args.file_path());
        fd = open(path.c_str(), O_WRONLY | O_SYNC);

        lseek(fd, args.offset(), SEEK_SET);
        bytes_written = write(fd, buffer, bytes_recv);
        if (bytes_written == -1) {
            perror("write");
            ret->set_err(errno);
            ret->set_size(-1);
            return Status::OK;
        }

        close(fd);
        delete buffer;

        ret->set_size(bytes_written);
        ret->set_err(0);

        return Status::OK;
    }


    Status WatFSTruncate(ServerContext *context, const WatFSTruncateArgs *args,
                       WatFSTruncateRet *ret) override {

        string file_path;

        int err;

        file_path = translate_pathname(args->file_path());

        err = truncate(file_path.c_str(), args->size());
        if (err == -1) {
            perror("truncate");
        }

        ret->set_err(err);

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
        string file_path;
        
        string marshalled_attr;
        string marshalled_dir_entry;

        WatFSReaddirRet ret;

        file_path = translate_pathname(args->file_handle());

        dh = opendir(file_path.c_str());
        dir_entry = readdir(dh);
        if (dir_entry == NULL) {
            cerr << "DEBUG: readdir - null dir_entry!" << endl;
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
            stat(dir_entry->d_name, &attr);
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
    Status WatFSMknod(ServerContext *context, const WatFSMknodArgs *args,
                       WatFSMknodRet *ret) override {

        string path;
        mode_t mode;
        dev_t rdev;

        int err;

        path = translate_pathname(args->path());
        mode = args->mode();
        rdev = args->rdev();

        if (S_ISFIFO(mode)) {
            err = mkfifo(path.c_str(), mode);
        } else {
            err = mknod(path.c_str(), mode, rdev);
        }


        if (err == -1) {
            ret->set_err(errno);
            cout << "DEBUG: mknod - " << path << endl;
            perror("mknod");
        } else {
            ret->set_err(0);
        }

        return Status::OK;
    }


    /*
     *
     */
    Status WatFSUnlink(ServerContext *context, const WatFSUnlinkArgs *args,
                       WatFSUnlinkRet *ret) override {

        string path;

        path = translate_pathname(args->path());

        int err = unlink(path.c_str());

        if (err == -1) {
            ret->set_err(errno);
            perror("unlink");
            cout << "DEBUG: unlink - " << path << endl;
        } else {
            ret->set_err(0);
        }

        return Status::OK;
    }


    /*
     *
     */
    Status WatFSRename(ServerContext *context, const WatFSRenameArgs *args,
                       WatFSRenameRet *ret) override {

        string source_path;
        string dest_path;

        source_path = translate_pathname(args->source());
        dest_path = translate_pathname(args->dest());

        int err = rename(source_path.c_str(), dest_path.c_str());

        if (err == -1) {
            ret->set_err(errno);
            perror("rename");
            cout << "DEBUG: rename - " << source_path << endl;
        } else {
            ret->set_err(0);
        }

        return Status::OK;
    }


    /*
     *
     */
    Status WatFSMkdir(ServerContext *context, const WatFSMkdirArgs *args,
                       WatFSMkdirRet *ret) override {

        string path;

        path = translate_pathname(args->path());

        int err = mkdir(path.c_str(), args->mode());

        if (err == -1) {
            ret->set_err(errno);
            perror("rmdir");
            cout << "DEBUG: rmdir - " << path << endl;
        } else {
            ret->set_err(0);
        }

        return Status::OK;
    }


    /*
     *
     */
    Status WatFSRmdir(ServerContext *context, const WatFSRmdirArgs *args,
                       WatFSRmdirRet *ret) override {

        string path;

        path = translate_pathname(args->path());

        int err = rmdir(path.c_str());

        if (err == -1) {
            ret->set_err(errno);
            perror("rmdir");            
            cout << "DEBUG: rmdir - " << path << endl;

        } else {
            ret->set_err(0);
        }

        return Status::OK;
    }


    /*
     *
     */
    Status WatFSUtimens(ServerContext *context, const WatFSUtimensArgs *args,
                       WatFSUtimensRet *ret) override {

        string path;
        struct timespec ts[2];

        path = translate_pathname(args->path());

        ts[0].tv_sec = args->ts_access_sec();
        ts[0].tv_nsec = args->ts_access_nsec();
        ts[1].tv_sec = args->ts_modify_sec();
        ts[1].tv_nsec = args->ts_modify_nsec();

        // update timestamp, path is relative to current working directory
        int err = utimensat(AT_FDCWD, path.c_str(), ts, AT_SYMLINK_NOFOLLOW);

        if (err == -1) {
            ret->set_err(errno);
            perror("utimensat");
            cout << "DEBUG: path - " << path << endl;
        } else {
            ret->set_err(0);
        }

        return Status::OK;
    }


private:
    string root_directory;

    string translate_pathname(const string &pathname) {
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
