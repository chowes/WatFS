#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>


static struct fuse_operations watfs_oper;


/*
 * Command line options
 *
 * show help
 * -h, --help
 */
static struct options {
    const char *mount_point;
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


static void *watfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
    (void) conn;

    // turn off kernel caching, we want to flush file contents on open 
    cfg->kernel_cache = 0;

    return NULL;
}


// TODO: implement this properly, needs to have the same behaviour as stat
static int watfs_getattr(const char *path, struct stat *stbuf, 
                         struct fuse_file_info *fi)
{
    (void) fi;
    int res = 0;

    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;

    } else if (strcmp(path, "test") == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = 10;
    } else
        res = -ENOENT;

    return res;
}


/*
 * g++ complains about struct initialization, so this just initializes our
 * fuse_operations struct with the procedures we've defined
 */
void set_operations(struct fuse_operations *ops)
{
    ops->init        = watfs_init;
    ops->getattr     = watfs_getattr;
}


int main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    /* set defaults */
    options.mount_point = strdup(".");

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
