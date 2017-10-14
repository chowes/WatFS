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
using watfs::WatFSReadArgs;
using watfs::WatFSReadRet;
using watfs::WatFSWriteArgs;
using watfs::WatFSWriteRet;
using watfs::WatFSReaddirArgs;
using watfs::WatFSReaddirRet;


using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
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

        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());


        client_status.set_status(0);

        Status status = stub_->WatFSNull(&context, client_status, 
                                         &server_status);

        if (!status.ok() || server_status.status() != client_status.status()) {
            cerr << status.error_message() << endl;
            return false;
        }

        return true;
    }


    /*
     * get attribute information from the server. 
     *
     * fills in the given struct stat with values for relevant file
     * 
     * returns 0 on success, or -1 if the gRPC call fails. errno is set on error
     */
    int WatFSGetAttr(string filename, struct stat *statbuf) {
        ClientContext context;
        WatFSGetAttrArgs getattr_args;
        WatFSGetAttrRet getattr_ret;

        string marshalled_attr;

        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());


        getattr_args.set_file_path(filename);

        Status status = stub_->WatFSGetAttr(&context, getattr_args, 
                                            &getattr_ret);

        if (!status.ok()) {
            errno = ETIMEDOUT;
            cerr << status.error_message() << endl;
            return -1;
        }

        marshalled_attr = getattr_ret.attr();
        memset(statbuf, 0, sizeof(struct stat));
        memcpy(statbuf, marshalled_attr.data(), sizeof(struct stat));
        

        // on error we set errno and return -1
        if (getattr_ret.err() != 0) {
            errno = getattr_ret.err();
            return -1;
        } else {
            return 0;
        }
        
    }


    /*
     * get a file handle from the server. 
     *
     * given the handle of a directory to search and a file name, returns a
     * file handle (the server representation of the file path), and fills in
     * a struct stat with file attributes, and another struct stat with
     * attributes for the containing directory 
     * 
     * returns 0 on success, or -1 on failure, errno is set on error.
     */
    int WatFSLookup(const string &dir, const string &file, string &file_handle,
                    struct stat *file_stat, struct stat *dir_stat) {

        ClientContext context;
        WatFSLookupArgs lookup_args;
        WatFSLookupRet lookup_ret;

        string marshalled_file_handle;
        string marshalled_file_attr;
        string marshalled_dir_attr;

        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());


        lookup_args.set_dir_handle(dir);
        lookup_args.set_file_name(file);

        Status status = stub_->WatFSLookup(&context, lookup_args, &lookup_ret);

        if (!status.ok()) {
            errno = ETIMEDOUT;
            cerr << status.error_message() << endl;
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

        // on error we set errno and return -1
        if (lookup_ret.err() != 0) {
            errno = lookup_ret.err();
            return -1;
        } else {
            return 0;
        }
    }


    /*
     * read from a file stored on the server. 
     *
     * Given a WatFS file handle (string containing server file path), we read
     * requested number of bytes from the file on the server into the given
     * buffer. Given boolean eof is set to indicate eof, and a struct stat is
     * populated with the attributes of the read file. 
     * 
     * returns number of bytes read into the buffer on success, or -1 on error.
     * errno is set on error.
     */
    int WatFSRead(const string &file_handle, int offset, int count, bool &eof,
                  struct stat *file_stat, char *data) {

        ClientContext context;
        WatFSReadArgs read_args;
        WatFSReadRet read_ret;
        
        string marshalled_file_attr;
        string marshalled_data;

        int bytes_read = 0;

        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());


        read_args.set_file_handle(file_handle);
        read_args.set_offset(offset);
        read_args.set_count(count);


        unique_ptr<ClientReader<WatFSReadRet>> reader(
            stub_->WatFSRead(&context, read_args));
        

        // read requested data from stream
        while (reader->Read(&read_ret) && bytes_read < count) {
            
            // fail as soon as we find a problem
            if (read_ret.count() == -1) {
                errno = read_ret.err();
                return -1;
            }

            marshalled_file_attr = read_ret.file_attr();
            marshalled_data = read_ret.data();
            
            // doing this every time is kind of inefficient
            memset(file_stat, 0, sizeof(struct stat));
            memcpy(file_stat, marshalled_file_attr.data(), sizeof(struct stat));
         
            // assume alloc'd correctly in caller
            memcpy(data+bytes_read, marshalled_data.data(), read_ret.count());

            eof = read_ret.eof();

            bytes_read += read_ret.count();
        }

        Status status = reader->Finish();

        if (!status.ok()) {
            errno = ETIMEDOUT;
            cerr << status.error_message() << endl;
            return -1;
        }

        if (read_ret.count() == -1) {
            errno = read_ret.err();
        }

        // on error we set errno and return -1
        if (read_ret.err() != 0) {
            errno = read_ret.err();
            return -1;
        } else {
            return bytes_read;
        }
    }


    /*
     * 
     */
    int WatFSWrite() {

    }


    /*
     * 
     */
    int WatFSReaddir() {

    }


private:
    unique_ptr<WatFS::Stub> stub_;

    /*
     * We want to get an absolute deadline for our grpc calls, since we're using
     * wait_for_ready semantics. I have this set to 10 seconds, maybe we can 
     * make this configurable?
     */
    chrono::system_clock::time_point GetDeadline() {
        return chrono::system_clock::now() + chrono::seconds(10);
    }
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
        perror("Client::WatFSGetAttr");
    } else {
        cout << file_attr.st_size << endl;
    }


    if (err = client.WatFSLookup(argv[1], argv[2], path, &file_attr, &dir_attr)) {
        perror("Client::WatFSLookup");
    } else {
        cout << path << endl;
    }


    bool eof;
    char *data = (char*)malloc(100000);
    path = argv[1];
    path += argv[2];
    if (client.WatFSRead(path, 0, 100000, eof, &file_attr, data) == -1) {
        perror("Client::WatFSRead");
    } else {
        cout << data << endl;
    }

    return 0;
}
