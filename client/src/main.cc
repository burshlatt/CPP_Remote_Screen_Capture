#include <iostream>

#include "client.h"
#include "input_parser.h"

int main(int argc, char* argv[]) {
    try {
        InputParser parser(ProgramType::K_CLIENT);
        parser.Parse(argc, argv);

        std::string host{parser.GetHost()};
        uint16_t port{parser.GetPort()};
        unsigned period{parser.GetPeriod()};

        Client client(host, port, period);
        client.Run();
    } catch (const std::invalid_argument& ex) {
        std::cerr << ex.what() << '\n';
    }

    return 0;
}
