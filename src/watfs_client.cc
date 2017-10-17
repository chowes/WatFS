#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>

#include "watfs_grpc_client.h"


static struct fuse_operations watfs_oper;


static struct options { 
    int show_help;
} options;

#define OPTION(t, p)                           \
    { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
    OPTION("-h", show_help),
    OPTION("--help", show_help),
    FUSE_OPT_END
};

static void show_help(const char *progname)
{
    std::cout << "usage: " << progname << " [-s -d] <mountpoint>\n\n";
}


void *watfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
    (void) conn;

    WatFSClient *client = new WatFSClient(grpc::CreateChannel("0.0.0.0:50051", 
                                    grpc::InsecureChannelCredentials()), 30);

    client->verf = client->WatFSNull();

    return client;
} 


int watfs_getattr(const char *path, struct stat *stbuf,
                          struct fuse_file_info *fi)
{    
    int res;

    WatFSClient *client = (WatFSClient *)fuse_get_context()->private_data;
    res = client->WatFSGetAttr(path, stbuf);

    return res;
}


int watfs_opendir(const char *path, struct fuse_file_info *f)
{   
    int res;
    
    WatFSClient *client = (WatFSClient *)fuse_get_context()->private_data;

    res = client->WatFSLookup(path);

    return res;
}


int watfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
                  off_t offset, struct fuse_file_info *fi, 
                  enum fuse_readdir_flags flags)
{
    int res;
    
    WatFSClient *client = (WatFSClient *)fuse_get_context()->private_data;

    res = client->WatFSReaddir(path, buf, filler);
    
    return res;
}


int watfs_open(const char *path, struct fuse_file_info *f)
{    
    int res;
    
    WatFSClient *client = (WatFSClient *)fuse_get_context()->private_data;

    res = client->WatFSLookup(path);

    return 0;
}


int watfs_rename(const char* from, const char* to, unsigned int flags)
{
    int res;

    WatFSClient *client = (WatFSClient *)fuse_get_context()->private_data;

    res = client->WatFSRename(from, to);
    
    return res;
}


int watfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res;

    WatFSClient *client = (WatFSClient *)fuse_get_context()->private_data;

    res = client->WatFSMknod(path, mode, rdev);
    
    return res;
}


int watfs_read(const char* path, char *buf, size_t size, off_t offset, 
               struct fuse_file_info* fi) {

    int res;

    WatFSClient *client = (WatFSClient *)fuse_get_context()->private_data;

    res = client->WatFSRead(path, offset, size, buf);
    
    return res;
}


int watfs_write(const char* path, const char *buf, size_t size, off_t offset, 
                struct fuse_file_info* fi) {

    int res;
    string marshalled_path;
    string marshalled_data;

    marshalled_path.assign(path);
    marshalled_data.assign(buf);
    
    WatFSClient *client = (WatFSClient *)fuse_get_context()->private_data;

    CommitData commit_data(path, offset, size, marshalled_data);
    client->cached_writes.push_back(commit_data);

    res = client->WatFSWrite(path, buf, size, offset);
    
    return res;
}


int watfs_commit(const char* path, struct fuse_file_info *fi) {

    int res;

    WatFSClient *client = (WatFSClient *)fuse_get_context()->private_data;

    int verf = client->WatFSCommit();

    if (verf != client->verf) {
        cerr << "Server crashed! Resend cached writes" << endl;
        cerr << "our verf: " << client->verf << endl;
        cerr << "server verf: " << verf << endl;
        client->verf = verf;
    }

    while (client->verf != verf) {
        for (auto write : client->cached_writes) {
            client->WatFSWrite(write.path.data(), write.data.data(), 
                               write.size, write.offset);
        }
        verf = client->WatFSCommit();
    }
    
    client->cached_writes.clear();

    // we aren't allowed to return errors here!
    return 0;
}


int watfs_flush(const char* path, struct fuse_file_info *fi) {
    
    return watfs_commit(path, fi);
}


int watfs_truncate(const char* path, off_t size, struct fuse_file_info *fi) {

    int res;

    WatFSClient *client = (WatFSClient *)fuse_get_context()->private_data;

    res = client->WatFSTruncate(path, size);
    
    return res;
}


int watfs_unlink(const char* path)
{
    int res;

    WatFSClient *client = (WatFSClient *)fuse_get_context()->private_data;

    res = client->WatFSUnlink(path);
    
    return res;
}


int watfs_mkdir(const char* path, mode_t mode)
{
    int res;

    WatFSClient *client = (WatFSClient *)fuse_get_context()->private_data;

    res = client->WatFSMkdir(path, mode);

    return res;
}


int watfs_rmdir(const char* path)
{
    int res;

    WatFSClient *client = (WatFSClient *)fuse_get_context()->private_data;

    res = client->WatFSRmdir(path);
    
    return res;
}


int watfs_utimens(const char *path, const struct timespec tv[2], 
                  struct fuse_file_info *fi)
{
    int res;

    WatFSClient *client = (WatFSClient *)fuse_get_context()->private_data;

    res = client->WatFSUtimens(path, tv[0], tv[1]);
    
    return res;
}


void set_fuse_ops(struct fuse_operations *ops) {
    ops->init       = watfs_init;
    ops->getattr    = watfs_getattr;
    ops->opendir    = watfs_opendir;
    ops->readdir    = watfs_readdir;
    ops->mknod      = watfs_mknod;
    ops->open       = watfs_open;
    ops->mknod      = watfs_mknod;
    ops->read       = watfs_read;
    ops->write      = watfs_write;
    ops->release    = watfs_commit;
    ops->flush      = watfs_flush;
    ops->truncate   = watfs_truncate;
    ops->rename     = watfs_rename;
    ops->unlink     = watfs_unlink;
    ops->mkdir      = watfs_mkdir;
    ops->rmdir      = watfs_rmdir;
    ops->utimens    = watfs_utimens;
}


int main(int argc, char* argv[]){

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1) {
        return 1;
    }

    if (options.show_help) {
        show_help(argv[0]);
        assert(fuse_opt_add_arg(&args, "--help") == 0);
        args.argv[0] = (char*) "";
    }

    set_fuse_ops(&watfs_oper);

    return fuse_main(argc, argv, &watfs_oper, &options);
}



