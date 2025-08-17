#include <iostream>

#include "server.h"
#include "input_parser.h"

int main(int argc, char* argv[]) {
    try {
        InputParser parser(ProgramType::K_SERVER);
        parser.Parse(argc, argv);

        uint16_t port{parser.GetPort()};

        Server server(port);
        server.Run();
    } catch (const std::invalid_argument& ex) {
        std::cerr << ex.what() << '\n';
    }

    return 0;
}
