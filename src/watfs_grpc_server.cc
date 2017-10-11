#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>

#include <grpc++/grpc++.h>





static void print_usage()
{
    std::cout << "usage: ./watfs_grpc_server [options] <rootdir>" << std::endl;
}


int main(int argc, const char *argv[])
{
    const char *root_dir;

    // we don't have any options yet, just ignore them for now
    while (getopt(argc, (char **)argv, "abc:") != -1);
    root_dir = argv[optind++];

    if (root_dir == NULL) {
        print_usage();
    }

    std::cout << root_dir << std::endl;

    return 0;
}