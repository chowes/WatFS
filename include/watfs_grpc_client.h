#define FUSE_USE_VERSION 30

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fuse.h>

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
using watfs::WatFSStatus;
using watfs::WatFSStatus;
using watfs::WatFSStatus;
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

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientWriter;
using grpc::Status;

using namespace std;


#ifndef __WATFS_GRPC_CLIENT__
#define __WATFS_GRPC_CLIENT__


// This is the recommended message size for streams
#define MESSAGE_SZ          16384


class WatFSClient {
public:
    
    /*
     * Constructor using default deadline
     */
    WatFSClient(shared_ptr<Channel> channel);


    /*
     * Constructor setting gRPC call deadline in seconds
     */
    WatFSClient(shared_ptr<Channel> channel, long deadline);


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
     * given the handle of a directory to search and a file name.
     * 
     * returns 0 on success, or -1 on failure, errno is set on error.
     */
    int WatFSLookup(const string &path);


    /*
     * read from a file stored on the server. 
     *
     * Given a WatFS file handle (string containing server file path), we read
     * requested number of bytes from the file on the server into the given
     * buffer.
     * 
     * returns number of bytes read into the buffer on success, or -1 on error.
     * errno is set on error.
     */
    int WatFSRead(const string &file_handle, int offset, int count, char *data);


    /*
     * write to a file stored on the server
     *
     * Given a WatFS file handle (string containing server file path), we write
     * requested number of bytes to the file on the server at the specified 
     * offset from the given buffer. If commit is set to true, sync is called 
     * and data is commited to disk. An error field is sent back to the 
     * client on error, set to relevant errno. 
     * 
     * returns number of bytes read into the buffer on success, or -1 on error.
     * errno is set on error.
     */
    int WatFSWrite(const string &file_handle, int offset, int count, bool flush,
                   const char *data);


    /*
     * 
     */
    int WatFSReaddir(const string &file_handle, void *buffer, 
                     fuse_fill_dir_t filler);


    /*
     * 
     */
    int WatFSCreate(const string &path, mode_t mode);


    /*
     * 
     */
    int WatFSUnlink(const string &path);


    /*
     * 
     */
    int WatFSRename(const string &from, const string &to);


    /*
     * 
     */
    int WatFSMkdir(const string &path, mode_t mode);


    /*
     * 
     */
    int WatFSRmdir(const string &path);


    /*
     * 
     */
    int WatFSCommit();


private:
    unique_ptr<WatFS::Stub> stub_;

    // deadline for gRPC calls in seconds
    long grpc_deadline;

    /*
     * We want to get an absolute deadline for our grpc calls, since we're using
     * wait_for_ready semantics.
     */
    chrono::system_clock::time_point GetDeadline() {
        return chrono::system_clock::now() + chrono::seconds(grpc_deadline);
    }
};

#endif // __WATFS_GRPC_CLIENT__
