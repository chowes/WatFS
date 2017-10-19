#include <string>
#include <vector>

using namespace std;

#ifndef __WATFS_COMMIT_DATA__
#define __WATFS_COMMIT_DATA__

class CommitData {
public:
    string path;
    long offset;
    long size;
    string data;

    CommitData(const char *new_path, long new_offset, long new_size, 
               const char *new_data) {

        path.assign(new_path);
        offset = new_offset;
        size = new_size;
        data.assign(new_data, size);
    }
};

#endif // __WATFS_COMMIT_DATA__
