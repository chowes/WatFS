#include "watfs_grpc_client.h"


WatFSClient::WatFSClient(shared_ptr<Channel> channel) : 
    stub_(WatFS::NewStub(channel)) {}


WatFSClient::WatFSClient(shared_ptr<Channel> channel, long deadline) : 
    stub_(WatFS::NewStub(channel)) {

        grpc_deadline = deadline;
    }


bool WatFSClient::WatFSNull() {
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


int WatFSClient::WatFSGetAttr(string filename, struct stat *statbuf) {
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


int WatFSClient::WatFSLookup(const string &dir, const string &file, 
                             string &file_handle,struct stat *file_stat, 
                             struct stat *dir_stat) {

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


int WatFSClient::WatFSRead(const string &file_handle, int offset, int count, 
                           bool &eof, struct stat *file_stat, char *data) {

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


int WatFSClient::WatFSWrite() {

}


int WatFSClient::WatFSReaddir() {

}
