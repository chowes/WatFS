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


static int watfs_getattr(const char *path, struct stat *stbuf,
                          struct fuse_file_info *fi)
{    
    int res;
    WatFSClient client(grpc::CreateChannel("0.0.0.0:50051", 
                       grpc::InsecureChannelCredentials()));

    res = client.WatFSGetAttr(path, stbuf);

    return res;
}


int watfs_opendir(const char *path, struct fuse_file_info *f)
{    
    return 0;
}


int watfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
                  off_t offset, struct fuse_file_info *fi, 
                  enum fuse_readdir_flags flags)
{
    int res;
    WatFSClient client(grpc::CreateChannel("0.0.0.0:50051", 
                       grpc::InsecureChannelCredentials()));

    res = client.WatFSReaddir(path, buf, filler);
    
    return res;
}


void set_fuse_ops(struct fuse_operations *ops) {
    ops->getattr = watfs_getattr;
    ops->opendir = watfs_opendir;
    ops->readdir = watfs_readdir;
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



