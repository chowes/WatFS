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
    WatFSClient(shared_ptr<Channel> channel);


    /* 
     * call WatFSNull to ping the server
     *
     * return: response from server on success
     *         -1 on failure
     */
    bool WatFSNull();


    /*
     * get attribute information from the server. 
     *
     * fills in the given struct stat with values for relevant file
     * 
     * returns 0 on success, or -1 if the gRPC call fails. errno is set on error
     */
    int WatFSGetAttr(string filename, struct stat *statbuf);


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
                    struct stat *file_stat, struct stat *dir_stat);


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
                  struct stat *file_stat, char *data);


    /*
     * 
     */
    int WatFSWrite();


    /*
     * 
     */
    int WatFSReaddir();


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
