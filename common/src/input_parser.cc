#include <iostream>

#include <arpa/inet.h>

#include "input_parser.h"

InputParser::InputParser(ProgramType type) :
    _prog_type(type)
{
    if (_prog_type == ProgramType::K_SERVER) {
        InitServerStructs();
    } else if (_prog_type == ProgramType::K_CLIENT) {
        InitClientStructs();
    }
}

void InputParser::InitServerStructs() {
    _long_options = {
        {"port", required_argument, nullptr, 0},
        {nullptr, 0, nullptr, 0}
    };

    _option_enabled_ht = {
        { "--port", false }
    };
}

void InputParser::InitClientStructs() {
    _long_options = {
        {"srv", required_argument, nullptr, 0},
        {"period", required_argument, nullptr, 0},
        {nullptr, 0, nullptr, 0}
    };

    _option_enabled_ht = {
        { "--srv", false },
        { "--period", false }
    };
}

std::string InputParser::GetHost() const noexcept {
    return _host;
}

uint16_t InputParser::GetPort() const noexcept {
    return _port;
}

unsigned InputParser::GetPeriod() const noexcept {
    return _period;
}

void InputParser::ParseSrv(char* arg) {    
    std::string host_port(arg);

    auto pos{host_port.find(":")};
    
    if (pos == std::string::npos) {
        throw std::invalid_argument("Invalid host or port: " + host_port);
    }

    auto host_end_it{host_port.begin() + pos};
    auto port_beg_it{host_port.begin() + pos + 1};

    std::string host_str(host_port.begin(), host_end_it);
    std::string port_str(port_beg_it, host_port.end());

    ParseHost(host_str.data());
    ParsePort(port_str.data());
}

void InputParser::ParseHost(char* arg) {
    std::string host_str(arg);

    struct in_addr addr;

    if (inet_pton(AF_INET, host_str.c_str(), &addr) != 1) {
        throw std::invalid_argument("Invalid host.");
    }

    _host = host_str;
}

int InputParser::ParseNum(const std::string& num_str) {
    size_t count{};
    int num{std::stoi(num_str, &count)};

    if (count != num_str.size()) {
        throw std::invalid_argument("Invalid num.");
    }

    return num;
}

void InputParser::ParsePort(char* arg) {
    std::string port_str(arg);

    int port{ParseNum(port_str)};

    if (port <= 0 || port > 65535) {
        throw std::invalid_argument("Invalid port.");
    }

    _port = port;
}

void InputParser::ParsePeriod(char* arg) {
    std::string period_str(arg);

    int period{ParseNum(period_str)};

    if (period < 0 || period > 86400) {
        throw std::invalid_argument("Invalid period.");
    }

    _period = period;
}

void InputParser::HandleServerOption(int opt_index) {
    switch (opt_index) {
        case 0:
            ParsePort(optarg);
            break;
        default:
            return;
    }
}

void InputParser::HandleClientOption(int opt_index) {
    switch (opt_index) {
        case 0:
            ParseSrv(optarg);
            break;
        case 1:
            ParsePeriod(optarg);
            break;
        default:
            return;
    }
}

void InputParser::Parse(int argc, char* argv[]) {
    int option_index{0};

    while (getopt_long(argc, argv, "", _long_options.data(), &option_index) != -1) {
        std::string option(argv[optind - (optarg ? 2 : 1)]);
        auto it{_option_enabled_ht.find(option)};

        if (it == _option_enabled_ht.end()) {
            throw std::invalid_argument("Invalid option: " + option);
        }

        if (it->second == true) {
            throw std::invalid_argument("Duplicate option: " + it->first);
        }

        if (_prog_type == ProgramType::K_SERVER) {
            HandleServerOption(option_index);
        } else if (_prog_type == ProgramType::K_CLIENT) {
            HandleClientOption(option_index);
        }

        it->second = true;
    }

    std::string missing_options;

    for (const auto& [opt, enabled] : _option_enabled_ht) {
        if (!enabled) {
            missing_options += opt + " ";
        }
    }

    if (!missing_options.empty()) {
        throw std::invalid_argument("Missing options: " + missing_options);
    }
}
