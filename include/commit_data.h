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

    CommitData(string new_path, long new_offset, long new_size, 
               string new_data) {

        path = new_path;
        offset = new_offset;
        size = new_size;
        data.assign(new_data.data());
    }
};

#endif // __WATFS_COMMIT_DATA__
