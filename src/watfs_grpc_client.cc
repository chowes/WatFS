#include <iostream>
#include <memory>
#include <string>

#include <boost/program_options.hpp>
#include <grpc++/grpc++.h>



int main(int argc, const char *argv[])
{
    try {
        options_description desc{"Options"};
        desc.add_options()
        ("help,h", "Help screen")
        ("pi", value<float>()->default_value(3.14f), "Pi")
        ("age", value<int>()->notifier(on_age), "Age");

        variables_map vm;
        store(parse_command_line(argc, argv, desc), vm);
        notify(vm);

        if (vm.count("help"))
            std::cout << desc << '\n';
        else if (vm.count("age"))
            std::cout << "Age: " << vm["age"].as<int>() << '\n';
        else if (vm.count("pi"))
            std::cout << "Pi: " << vm["pi"].as<float>() << '\n';
    } catch (const error &ex) {
        std::cerr << ex.what() << '\n';
    }
    

    return 0;
}
