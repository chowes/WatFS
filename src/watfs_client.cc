#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


static struct fuse_operations watfs_oper;


/*
 * Command line options
 *
 * show help
 * -h, --help
 */
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
    printf("usage: %s [options] <mountpoint>\n\n", progname);
    printf("WatFS options:\n"
           "    --none=<s>             none\n"
           "\n");
}


// TODO: implement this properly, needs to have the same behaviour as stat
static int watfs_getattr(const char *path, struct stat *stat_buf, 
                         struct fuse_file_info *fi)
{
    (void) fi;
    int err;

    err = stat(path, stat_buf);
    stat_buf->st_uid = getuid();
    stat_buf->st_gid = getgid();

    return err;
}


static int readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info* fi) {
    return 0;
}


/*
 * g++ complains about struct initialization, so this just initializes our
 * fuse_operations struct with the procedures we've defined
 */
void set_operations(struct fuse_operations *ops)
{
    ops->getattr     = watfs_getattr;
}


int main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    /* Parse options */
    if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
        return 1;

    /* show our help message, then the fuse help message */
    if (options.show_help) {
        show_help(argv[0]);
        assert(fuse_opt_add_arg(&args, "--help") == 0);
        args.argv[0] = (char*) "";
    }

    set_operations(&watfs_oper);

    return fuse_main(args.argc, args.argv, &watfs_oper, NULL);
}
