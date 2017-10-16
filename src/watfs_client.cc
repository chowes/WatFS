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
    int retry_limit;
    int errno_timed_out=-110;
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


static int watfs_getattr(const char *path, struct stat *stbuf,
                          struct fuse_file_info *fi)
{    
    int res=options.errno_timed_out;
    int retries=0;
    WatFSClient client(grpc::CreateChannel("0.0.0.0:50051", 
                   grpc::InsecureChannelCredentials()));
    while(res==options.errno_timed_out && retries++ <= options.retry_limit){
        res = client.WatFSGetAttr(path, stbuf);
    }

    return res;
}


int watfs_opendir(const char *path, struct fuse_file_info *f)
{   
    int res=options.errno_timed_out;
    int retries=0;
    WatFSClient client(grpc::CreateChannel("0.0.0.0:50051", 
                   grpc::InsecureChannelCredentials()));
    while(res==options.errno_timed_out && retries++ <= options.retry_limit){
        res = client.WatFSLookup(path);
    }

    return res;
}


int watfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
                  off_t offset, struct fuse_file_info *fi, 
                  enum fuse_readdir_flags flags)
{
    int res=options.errno_timed_out;
    int retries=0;
    WatFSClient client(grpc::CreateChannel("0.0.0.0:50051", grpc::InsecureChannelCredentials()));
    while(res==options.errno_timed_out && retries++ <= options.retry_limit){
        res = client.WatFSReaddir(path, buf, filler);
    }
    
    return res;
}


int watfs_open(const char *path, struct fuse_file_info *f)
{    
    int res=options.errno_timed_out;
    int retries=0;
    WatFSClient client(grpc::CreateChannel("0.0.0.0:50051", 
                       grpc::InsecureChannelCredentials()));
    
    while(res==options.errno_timed_out && retries++ <= options.retry_limit){
        res = client.WatFSLookup(path);
    }

    return 0;
}


int watfs_rename(const char* from, const char* to, unsigned int flags)
{
    int res=options.errno_timed_out;
    int retries=0;
    WatFSClient client(grpc::CreateChannel("0.0.0.0:50051", 
                       grpc::InsecureChannelCredentials()));

    while(res==options.errno_timed_out && retries++ <= options.retry_limit){
        res = client.WatFSRename(from, to);
    }
    
    return res;
}


int watfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res=options.errno_timed_out;
    int retries=0;
    WatFSClient client(grpc::CreateChannel("0.0.0.0:50051", 
                       grpc::InsecureChannelCredentials()));

    while(res==options.errno_timed_out && retries++ <= options.retry_limit){
        res = client.WatFSMknod(path, mode, rdev);
    }
    
    return res;
}


int watfs_read(const char* path, char *buf, size_t size, off_t offset, 
               struct fuse_file_info* fi) {

    int res=options.errno_timed_out;
    int retries=0;

    WatFSClient client(grpc::CreateChannel("0.0.0.0:50051", 
                       grpc::InsecureChannelCredentials()));

    while(res==options.errno_timed_out && retries++ <= options.retry_limit){
        res = client.WatFSRead(path, offset, size, buf);
    }
    
    return res;
}


int watfs_write(const char* path, const char *buf, size_t size, off_t offset, 
                struct fuse_file_info* fi) {

    int res=options.errno_timed_out;
    int retries=0;

    WatFSClient client(grpc::CreateChannel("0.0.0.0:50051", 
                       grpc::InsecureChannelCredentials()));

    while(res==options.errno_timed_out && retries++ <= options.retry_limit){
        res = client.WatFSWrite(path, offset, size, true, buf);
    }
    
    return res;
}

int watfs_unlink(const char* path)
{
    int res=options.errno_timed_out;
    int retries=0;
    WatFSClient client(grpc::CreateChannel("0.0.0.0:50051", 
                       grpc::InsecureChannelCredentials()));
    while(res==options.errno_timed_out && retries++ <= options.retry_limit){
        res = client.WatFSUnlink(path);
    }
    
    return res;
}


int watfs_mkdir(const char* path, mode_t mode)
{
    int res=options.errno_timed_out;
    int retries=0;
    WatFSClient client(grpc::CreateChannel("0.0.0.0:50051", 
                   grpc::InsecureChannelCredentials()));
    while(res==options.errno_timed_out && retries++ <= options.retry_limit){
        res = client.WatFSMkdir(path, mode);
    }
    return res;
}


int watfs_rmdir(const char* path)
{
    int res=options.errno_timed_out;
    int retries=0;
    WatFSClient client(grpc::CreateChannel("0.0.0.0:50051", 
                       grpc::InsecureChannelCredentials()));
    while(res==options.errno_timed_out && retries++ <= options.retry_limit){
        res = client.WatFSRmdir(path);
    }
    
    return res;
}


int watfs_utimens(const char *path, const struct timespec tv[2], 
                  struct fuse_file_info *fi)
{
    int res=options.errno_timed_out;
    int retries=0;
    WatFSClient client(grpc::CreateChannel("0.0.0.0:50051", 
                       grpc::InsecureChannelCredentials()));
    while(res==options.errno_timed_out && retries++ <= options.retry_limit)
        res = client.WatFSUtimens(path, tv[0], tv[1]);
    
    return res;
}


void set_fuse_ops(struct fuse_operations *ops) {
    ops->getattr    = watfs_getattr;
    ops->opendir    = watfs_opendir;
    ops->readdir    = watfs_readdir;
    ops->mknod      = watfs_mknod;
    ops->open       = watfs_open;
    ops->mknod      = watfs_mknod;
    ops->read       = watfs_read;
    ops->write      = watfs_write;
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
    options.retry_limit = 1000;
    if (options.show_help) {
        show_help(argv[0]);
        assert(fuse_opt_add_arg(&args, "--help") == 0);
        args.argv[0] = (char*) "";
    }

    set_fuse_ops(&watfs_oper);

    return fuse_main(argc, argv, &watfs_oper, &options);
}



