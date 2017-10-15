#include "watfs_grpc_client.h"


WatFSClient::WatFSClient(shared_ptr<Channel> channel) : 
    stub_(WatFS::NewStub(channel)) {
        cout << "Started" << endl;
    }


WatFSClient::WatFSClient(shared_ptr<Channel> channel, long deadline) : 
    stub_(WatFS::NewStub(channel)) {
        grpc_deadline = deadline;
    }


bool WatFSClient::WatFSNull() {
    ClientContext context;
    WatFSStatus client_status;
    WatFSStatus server_status;

    client_status.set_status(0);

    Status status = stub_->WatFSNull(&context, client_status, 
                                     &server_status);

    if (!status.ok() || server_status.status() != client_status.status()) {
        cerr << status.error_message() << endl;
        return false;
    }

    return true;
}


int WatFSClient::WatFSGetAttr(string filename, struct stat *statbuf) {
    ClientContext context;
    WatFSGetAttrArgs getattr_args;
    WatFSGetAttrRet getattr_ret;

    string marshalled_attr;

    getattr_args.set_file_path(filename);

    Status status = stub_->WatFSGetAttr(&context, getattr_args, 
                                        &getattr_ret);

    if (!status.ok()) {
        errno = ETIMEDOUT;
        return -errno;
    }

    marshalled_attr = getattr_ret.attr();
    memset(statbuf, 0, sizeof(struct stat));
    memcpy(statbuf, marshalled_attr.data(), sizeof(struct stat));
    

    // on error we set errno and return -errno
    if (getattr_ret.err() != 0) {
        errno = getattr_ret.err();
        return -errno;
    } else {
        return 0;
    }
    
}


int WatFSClient::WatFSLookup(const string &path) {

    ClientContext context;
    WatFSLookupArgs lookup_args;
    WatFSLookupRet lookup_ret;

    string marshalled_file_handle;

    lookup_args.set_file_path(path);

    Status status = stub_->WatFSLookup(&context, lookup_args, &lookup_ret);

    if (!status.ok()) {
        errno = ETIMEDOUT;
        cerr << status.error_message() << endl;
        return -errno;
    }

    // on error we set errno and return -errno
    if (lookup_ret.err() != 0) {
        errno = lookup_ret.err();
        return -errno;
    } else {
        return 0;
    }
}


int WatFSClient::WatFSRead(const string &file_handle, int offset, int count, 
                           char *data) {

    ClientContext context;
    WatFSReadArgs read_args;
    WatFSReadRet read_ret;
    
    string marshalled_file_attr;
    string marshalled_data;

    int bytes_read = 0;

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
            return -errno;
        }
        marshalled_data = read_ret.data();
     
        // assume alloc'd correctly in caller
        memcpy(data+bytes_read, marshalled_data.data(), read_ret.count());

        bytes_read += read_ret.count();
    }

    Status status = reader->Finish();

    if (!status.ok()) {
        errno = ETIMEDOUT;
        cerr << status.error_message() << endl;
        return -errno;
    }

    // on error we set errno and return -errno
    if (read_ret.err() != 0 || read_ret.count() == -1) {
        errno = read_ret.err();
        return -errno;
    } else {
        return bytes_read;
    }
}


int WatFSClient::WatFSWrite(const string &file_handle, int offset, int count,
                            bool flush, const char *data) {
    
    ClientContext context;
    WatFSWriteArgs write_args;
    WatFSWriteRet write_ret;

    string marshalled_data;
    string marshalled_file_attr;

    int bytes_sent = 0;

    unique_ptr<ClientWriter<WatFSWriteArgs>> writer(
        stub_->WatFSWrite(&context, &write_ret));

    write_args.set_file_handle(file_handle);

    cerr << write_args.file_handle() << endl;

    write_args.set_commit(flush);

    int msg_sz;
    while (bytes_sent < count) {
        msg_sz = min(MESSAGE_SZ, count - bytes_sent);
        marshalled_data.assign(data+bytes_sent, msg_sz); 

        write_args.set_offset(offset + bytes_sent);
        write_args.set_count(msg_sz);
        write_args.set_data(marshalled_data);

        if (!writer->Write(write_args)) {
            // TODO: does this imply an error? maybe we need to set errno...
            break;
        }

        bytes_sent += msg_sz;

        cerr << bytes_sent << endl;
    }

    writer->WritesDone();
    Status status = writer->Finish();

    if (!status.ok()) {
        errno = ETIMEDOUT;
        cerr << status.error_message() << endl;
        return -errno;
    }

    // on error we set errno and return -errno
    if (write_ret.err() != 0 || write_ret.count() == -1) {
        errno = write_ret.err();
        return -errno;
    } else {
        return bytes_sent;
    }
}


int WatFSClient::WatFSReaddir(const string &file_handle, void *buffer, 
                              fuse_fill_dir_t filler) {

    ClientContext context;
    WatFSReaddirArgs readdir_args;
    WatFSReaddirRet readdir_ret;
    
    string marshalled_attr;
    string marshalled_dir_entry;

    struct dirent dir_entry;
    struct stat attr;

    readdir_args.set_file_handle(file_handle);

    unique_ptr<ClientReader<WatFSReaddirRet>> reader(
        stub_->WatFSReaddir(&context, readdir_args));
    

    // read requested data from stream
    while (reader->Read(&readdir_ret)) {

        marshalled_attr = readdir_ret.attr();
        marshalled_dir_entry = readdir_ret.dir_entry();
        

        memset(&attr, 0, sizeof(struct stat));
        memcpy(&attr, marshalled_attr.data(), sizeof(struct stat));

        memset(&dir_entry, 0, sizeof(struct dirent));
        memcpy(&dir_entry, marshalled_dir_entry.data(), sizeof(struct stat));
        
        /* we add the entry from here so we don't have to deal with sending back
         * a list */
        filler(buffer, dir_entry.d_name, &attr, 0, FUSE_FILL_DIR_PLUS);
    }

    Status status = reader->Finish();

    if (!status.ok()) {
        errno = ETIMEDOUT;
        cerr << status.error_message() << endl;
        return -errno;
    }


    // on error we set errno and return -errno
    if (readdir_ret.err() != 0) {
        errno = readdir_ret.err();
        return -errno;
    } else {
        return 0;
    }
}


int WatFSClient::WatFSMknod(const string &path, mode_t mode, dev_t rdev) {
    ClientContext context;
    WatFSMknodArgs mknod_args;
    WatFSMknodRet mknod_ret;

    mknod_args.set_path(path);
    mknod_args.set_mode(mode);
    mknod_args.set_rdev(rdev);

    Status status = stub_->WatFSMknod(&context, mknod_args, &mknod_ret);

    if (!status.ok()) {
        errno = ETIMEDOUT;
        cerr << status.error_message() << endl;
        return -errno;
    }

    // on error we set errno and return -errno
    if (mknod_ret.err() != 0) {
        errno = mknod_ret.err();
        return -errno;
    } else {
        return 0;
    }
}


int WatFSClient::WatFSUnlink(const string &path) {
    ClientContext context;
    WatFSUnlinkArgs unlink_args;
    WatFSUnlinkRet unlink_ret;

    unlink_args.set_path(path);

    Status status = stub_->WatFSUnlink(&context, unlink_args, &unlink_ret);

    if (!status.ok()) {
        errno = ETIMEDOUT;
        cerr << status.error_message() << endl;
        return -errno;
    }

    // on error we set errno and return -errno
    if (unlink_ret.err() != 0) {
        errno = unlink_ret.err();
        return -errno;
    } else {
        return 0;
    }
}


int WatFSClient::WatFSRename(const string &from, const string &to) {
    ClientContext context;
    WatFSRenameArgs rename_args;
    WatFSRenameRet rename_ret;

    rename_args.set_source(from);
    rename_args.set_dest(to);

    Status status = stub_->WatFSRename(&context, rename_args, &rename_ret);

    if (!status.ok()) {
        errno = ETIMEDOUT;
        return -errno;
    }

    // on error we set errno and return -errno
    if (rename_ret.err() != 0) {
        errno = rename_ret.err();
        return -errno;
    } else {
        return 0;
    }
}


int WatFSClient::WatFSMkdir(const string &path, mode_t mode) {
    ClientContext context;
    WatFSMkdirArgs mkdir_args;
    WatFSMkdirRet mkdir_ret;

    mkdir_args.set_path(path);
    mkdir_args.set_mode(mode);

    Status status = stub_->WatFSMkdir(&context, mkdir_args, &mkdir_ret);

    if (!status.ok()) {
        errno = ETIMEDOUT;
        cerr << status.error_message() << endl;
        return -errno;
    }

    // on error we set errno and return -errno
    if (mkdir_ret.err() != 0) {
        errno = mkdir_ret.err();
        return -errno;
    } else {
        return 0;
    }
}


int WatFSClient::WatFSRmdir(const string &path) {
    ClientContext context;
    WatFSRmdirArgs rmdir_args;
    WatFSRmdirRet rmdir_ret;

    rmdir_args.set_path(path);

    Status status = stub_->WatFSRmdir(&context, rmdir_args, &rmdir_ret);

    if (!status.ok()) {
        errno = ETIMEDOUT;
        cerr << status.error_message() << endl;
        return -errno;
    }

    // on error we set errno and return -errno
    if (rmdir_ret.err() != 0) {
        errno = rmdir_ret.err();
        return -errno;
    } else {
        return 0;
    }
}

int WatFSClient::WatFSUtimens(const string &path, struct timespec tv_access, 
                              struct timespec tv_modify) {
    ClientContext context;
    WatFSUtimensArgs utimens_args;
    WatFSUtimensRet utimens_ret;

    utimens_args.set_path(path);
    utimens_args.set_ts_access_sec(tv_access.tv_sec);
    utimens_args.set_ts_access_nsec(tv_access.tv_nsec);
    utimens_args.set_ts_modify_sec(tv_modify.tv_sec);
    utimens_args.set_ts_modify_nsec(tv_modify.tv_nsec);

    Status status = stub_->WatFSUtimens(&context, utimens_args, &utimens_ret);

    if (!status.ok()) {
        errno = ETIMEDOUT;
        cerr << status.error_message() << endl;
        return -errno;
    }

    // on error we set errno and return -errno
    if (utimens_ret.err() != 0) {
        errno = utimens_ret.err();
        return -errno;
    } else {
        return 0;
    }
}

