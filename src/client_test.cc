#include "watfs_grpc_client.h"


int main(int argc, const char *argv[])
{
    struct stat file_attr;
    struct stat dir_attr;
    string path;
    int err;

    WatFSClient client(grpc::CreateChannel("192.168.1.2:8080", 
                                           grpc::InsecureChannelCredentials()));

    if (err = client.WatFSGetAttr(argv[1], &file_attr)) {
        perror("Client::WatFSGetAttr");
    } else {
        cout << file_attr.st_size << endl;
    }

    cout << client.WatFSNull() << endl;


    // if (err = client.WatFSLookup(argv[1], argv[2], path, &file_attr, &dir_attr)) {
    //     perror("Client::WatFSLookup");
    // } else {
    //     cout << path << endl;
    // }


    // bool eof;
    // char *data = (char*)malloc(20000);
    // path = argv[1];
    // path += argv[2];
    // if (client.WatFSRead(path, 0, 20000, eof, &file_attr, data) == -1) {
    //     perror("Client::WatFSRead");
    // } else {
    //     // cout << data << endl;
    // }
    
    // string write_data = "hello world my name is colin!";
    // if (client.WatFSWrite(path, 0, write_data.size(), true, &file_attr, write_data.data()) == -1) {
    //     perror("Client::WatFSWrite");
    // }

    // if (client.WatFSRead(path, 0, 20000, eof, &file_attr, data) == -1) {
    //     perror("Client::WatFSRead");
    // } else {
    //     // cout << data << endl;
    // }

    // client.WatFSReaddir("..", data, NULL);

    return 0;
}
