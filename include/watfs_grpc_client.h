#define FUSE_USE_VERSION 30

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fuse.h>
#include <pthread.h>

#include <iostream>
#include <memory>
#include <string>

#include <grpc++/grpc++.h>
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>

#include "commit_data.h"
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
using watfs::WatFSCommitArgs;
using watfs::WatFSCommitRet;
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

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientWriter;
using grpc::Status;

using namespace std;


#define MESSAGE_SZ          8192


#ifndef __WATFS_GRPC_CLIENT__
#define __WATFS_GRPC_CLIENT__


class WatFSClient {
public:

    // use to verify commits
    long verf;
    // data that has been written but not commited to disk
    vector<CommitData *> cached_writes;
    
    pthread_mutex_t cached_writes_mutex;

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
     * return: verf from server on success
     *         -1 on failure
     */
    long int WatFSNull();


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
     * offset from the given buffer. An error field is sent back to the 
     * client on error, set to relevant errno. 
     * 
     * returns number of bytes read into the buffer on success, or -1 on error.
     * errno is set on error.
     */
    int WatFSWrite(const string &file_handle, const char *buffer, long size,
                   long offset);


    int WatFSCommit();

    /*
     * 
     */
    int WatFSTruncate(const string &file_path, int size);


    /*
     * 
     */
    int WatFSReaddir(const string &file_handle, void *buffer, 
                     fuse_fill_dir_t filler);


    /*
     * 
     */
    int WatFSMknod(const string &path, mode_t mode, dev_t rdev);


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
    int WatFSUtimens(const string &path, struct timespec tv_access, 
                     struct timespec tv_modify);


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
