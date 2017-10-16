#include "watfs_grpc_client.h"


WatFSClient::WatFSClient(shared_ptr<Channel> channel) : 
    stub_(WatFS::NewStub(channel)) {
        grpc_deadline = 120;
    }


WatFSClient::WatFSClient(shared_ptr<Channel> channel, long deadline) : 
    stub_(WatFS::NewStub(channel)) {
        grpc_deadline = deadline;
    }


bool WatFSClient::WatFSNull() {
    WatFSStatus client_status;
    WatFSStatus server_status;

    client_status.set_status(0);

    Status status;

    do {    
        ClientContext context;
        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());

        status = stub_->WatFSNull(&context, client_status, 
                                  &server_status);
    } while (!status.ok());

    if (!status.ok() || server_status.status() != client_status.status()) {
        cerr << status.error_message() << endl;
        return false;
    }

    return true;
}


int WatFSClient::WatFSGetAttr(string filename, struct stat *statbuf) {
    WatFSGetAttrArgs getattr_args;
    WatFSGetAttrRet getattr_ret;

    string marshalled_attr;

    getattr_args.set_file_path(filename);

    Status status;

    do {
        ClientContext context;
        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());
        status = stub_->WatFSGetAttr(&context, getattr_args, 
                                     &getattr_ret);
    } while (!status.ok());

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

    WatFSLookupArgs lookup_args;
    WatFSLookupRet lookup_ret;

    string marshalled_file_handle;

    lookup_args.set_file_path(path);

    Status status;

    do {
        ClientContext context;
        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());
        status = stub_->WatFSLookup(&context, lookup_args, &lookup_ret);
    } while (!status.ok());

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

    WatFSReadArgs read_args;
    WatFSReadRet read_ret;

    string marshalled_file_attr;
    string marshalled_data;
    string buffer;

    int bytes_read;


    read_args.set_file_handle(file_handle);
    read_args.set_offset(offset);
    read_args.set_count(count);


    Status status;

    /*
     * This is a bunch of nonsense made necessary by a gRPC bug (issue 4475, fixed upstream)
     */
    do {
        bytes_read = 0;
        buffer.clear();

        ClientContext context;

        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());

        auto reader = stub_->WatFSRead(&context, read_args);
        
        // read requested data from stream
        while (reader->Read(&read_ret) && bytes_read < count) {
            
            // fail as soon as we find a problem
            if (read_ret.count() == -1) {
                errno = read_ret.err();
                break;
            }
            marshalled_data = read_ret.data();
            buffer.append(marshalled_data);

            // assume alloc'd correctly in caller
            bytes_read += read_ret.count();
        }

        status = reader->Finish();
    } while (!status.ok());

    memcpy(data, buffer.data(), bytes_read);

    if (!status.ok()) {
        errno = ETIMEDOUT;
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


int WatFSClient::WatFSWrite(const string &file_handle, const char *buffer, 
                            long total_size, long offset) {
    WatFSWriteArgs write_args;
    WatFSWriteRet write_ret;

    string marshalled_data;
    char *data = new char[total_size];
    memcpy(data, buffer, total_size);

    Status status;

    do {
        ClientContext context;
        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());

        auto writer = stub_->WatFSWrite(&context, &write_ret);

        int bytes_sent = 0;
        int msg_sz; // the size of the message sent over the stream
        while (bytes_sent < total_size) {
            // we want to send at most MESSAGE_SZ bytes at a time
            msg_sz = min(MESSAGE_SZ, (int)total_size - bytes_sent);
            // send this chunk over the stream
            
            marshalled_data.assign(data+bytes_sent, msg_sz);            
            cout << marshalled_data << endl;
            write_args.set_file_path(file_handle);
            write_args.set_buffer(marshalled_data);
            write_args.set_offset(offset);
            write_args.set_total_size(total_size);
            write_args.set_size(msg_sz);
            writer->Write(write_args);

            bytes_sent += msg_sz;
        }

        writer->WritesDone();
        Status status = writer->Finish();
    } while (!status.ok());

    if (!status.ok()) {
        errno = ETIMEDOUT;
        return -errno;
    }

    // on error we set errno and return -errno
    if (write_ret.err() != 0) {
        errno = write_ret.err();
        return -errno;
    } else {
        return write_ret.size();
    }

    return total_size;
}


int WatFSClient::WatFSTruncate(const string &file_path, int size) {
    WatFSTruncateArgs trunc_args;
    WatFSTruncateRet trunc_ret;

    trunc_args.set_file_path(file_path);
    trunc_args.set_size(size);

    Status status;

    do {
        ClientContext context;
        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());
        status = stub_->WatFSTruncate(&context, trunc_args, &trunc_ret);
    } while (!status.ok());

    if (!status.ok()) {
        errno = ETIMEDOUT;
        cerr << status.error_message() << endl;
        return -errno;
    }

    // on error we set errno and return -errno
    if (trunc_ret.err() != 0) {
        errno = trunc_ret.err();
        return -errno;
    } else {
        return 0;
    }
}


int WatFSClient::WatFSReaddir(const string &file_handle, void *buffer, 
                              fuse_fill_dir_t filler) {

    WatFSReaddirArgs readdir_args;
    WatFSReaddirRet readdir_ret;
    
    string marshalled_attr;
    string marshalled_dir_entry;

    struct dirent dir_entry;
    struct stat attr;

    Status status;

    readdir_args.set_file_handle(file_handle);

    do {
        ClientContext context;
        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());

        auto reader = stub_->WatFSReaddir(&context, readdir_args);

        // read requested directory data from stream
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

        status = reader->Finish();
    } while (!status.ok());

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
    WatFSMknodArgs mknod_args;
    WatFSMknodRet mknod_ret;

    mknod_args.set_path(path);
    mknod_args.set_mode(mode);
    mknod_args.set_rdev(rdev);

    Status status;

    do {
        ClientContext context;
        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());
        status = stub_->WatFSMknod(&context, mknod_args, &mknod_ret);
    } while (!status.ok());

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
    WatFSUnlinkArgs unlink_args;
    WatFSUnlinkRet unlink_ret;

    unlink_args.set_path(path);

    Status status;

    do {
        ClientContext context;
        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());
        status = stub_->WatFSUnlink(&context, unlink_args, &unlink_ret);
    } while (!status.ok());

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
    WatFSRenameArgs rename_args;
    WatFSRenameRet rename_ret;

    rename_args.set_source(from);
    rename_args.set_dest(to);

    Status status;

    do {
        ClientContext context;
        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());
        status = stub_->WatFSRename(&context, rename_args, &rename_ret);
    } while (!status.ok());

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
    WatFSMkdirArgs mkdir_args;
    WatFSMkdirRet mkdir_ret;

    mkdir_args.set_path(path);
    mkdir_args.set_mode(mode);

    Status status;

    do {
        ClientContext context;
        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());
        status = stub_->WatFSMkdir(&context, mkdir_args, &mkdir_ret);
    } while (!status.ok());

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
    WatFSRmdirArgs rmdir_args;
    WatFSRmdirRet rmdir_ret;

    rmdir_args.set_path(path);

    Status status;

    do {
        ClientContext context;
        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());
        status = stub_->WatFSRmdir(&context, rmdir_args, &rmdir_ret);
    } while (!status.ok());

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
    WatFSUtimensArgs utimens_args;
    WatFSUtimensRet utimens_ret;

    utimens_args.set_path(path);
    utimens_args.set_ts_access_sec(tv_access.tv_sec);
    utimens_args.set_ts_access_nsec(tv_access.tv_nsec);
    utimens_args.set_ts_modify_sec(tv_modify.tv_sec);
    utimens_args.set_ts_modify_nsec(tv_modify.tv_nsec);

    Status status;

    do {
        ClientContext context;
        context.set_wait_for_ready(true);
        context.set_deadline(GetDeadline());
        status = stub_->WatFSUtimens(&context, utimens_args, &utimens_ret);
    } while (!status.ok());

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

