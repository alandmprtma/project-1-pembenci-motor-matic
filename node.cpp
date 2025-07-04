#include "header/node.hpp"
#include <iostream>
#include "header/color.hpp"

void Node::run() {
    if (!connection) {
        throw std::runtime_error("Connection not initialized");
    }

    try {
        vector<uint8_t> buffer;
        buffer.resize(1460); // Preallocate buffer size
        while (true) {
            int32_t received = connection->recv(buffer, BUFFER_SIZE);
            
            if (received < 0) {
                std::cout << Color::RED << "[!]" << Color::RESET << " Receive error" << std::endl;
                break;
            } else if (received > 0) {
                handleMessage(buffer);
            }
            // Zero receive means connection closed
            else {
                std::cout << Color::YELLOW << "[i]" << Color::RESET << " Connection closed by peer" << std::endl;
                break;
            }
        }
    }
    catch (const std::exception& e) {
        std::cout << Color::RED << "[!]" << Color::RESET << " Node error: " << e.what() << std::endl;
    }
    
    connection->close();
}