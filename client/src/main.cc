#include <iostream>

#include "client.h"

int main(int argc, char** argv) {
    if (argc != 5 || std::string(argv[1]) != "--srv" || std::string(argv[3]) != "--period") {
        std::cerr << "Usage: " << argv[0] << " --srv <host>:<port> --period <timeout secs>\n";

        return 1;
    }

    try {
        Client client(argv[2], std::stoi(argv[4]));
        client.Run();
    } catch (const std::invalid_argument& ex) {
        std::cerr << ex.what() << '\n';
    }

    return 0;
}
