#include <iostream>

#include "server.h"

int main(int argc, char* argv[]) {
    if (argc != 3 || std::string(argv[1]) != "--port") {
        std::cerr << "Usage: " << argv[0] << " --port <listen port>\n";

        return 1;
    }

    try {
        Server server(argv[2]);
        server.Run();
    } catch (const std::invalid_argument& ex) {
        std::cerr << ex.what() << '\n';
    }

    return 0;
}
