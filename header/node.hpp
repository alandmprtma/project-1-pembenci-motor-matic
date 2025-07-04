#ifndef node_h
#define node_h

#include "socket.hpp"

/**
 * Abstract class.
 *
 * This is the base class for Server and Client class.
 */
class Node
{
protected:
    static const uint32_t BUFFER_SIZE = 1460; // MAX_SEGMENT_SIZE
    TCPSocket *connection;

public:
    void run();
    virtual void handleMessage(std::vector<uint8_t>& buffer) = 0;
};

#endif