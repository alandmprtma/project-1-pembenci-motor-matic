#ifndef socket_h
#define socket_h

#include <sys/socket.h>
#include <string>
#include <netinet/in.h>
#include <functional>
#include "segment.hpp"
#include "segment_handler.hpp"

using namespace std;

enum TCPStatusEnum
{
    CLOSED = -1,
    LISTEN = 0,
    SYN_SENT = 1,
    SYN_RECEIVED = 2,
    ESTABLISHED = 3,
    FIN_WAIT_1 = 4,
    FIN_WAIT_2 = 5,
    CLOSE_WAIT = 6,
    CLOSING = 7,
    LAST_ACK = 8,
    TIME_WAIT = 9
};

class TCPSocket
{
private:
    string ip;
    int32_t port;

    /**
     * Socket descriptor
     */
    int32_t socket;

    SegmentHandler *segmentHandler;

    TCPStatusEnum status;

    struct sockaddr_in peerAddr;

    bool peerAddrSet; 

    // Helper Method
    void setTimeout(int seconds);
    bool sendSegment(const Segment& segment, const struct sockaddr_in& addr);
    int32_t receiveSegment(Segment& segment, struct sockaddr_in& addr);

public:
    TCPSocket(string ip, int32_t port);

    ~TCPSocket();

    struct sockaddr_in createAddr(string ip, int32_t port);

    bool doHandshake(struct sockaddr_in& destAddr);

    void listen();

    void send(string ip, int32_t port, void *dataStream, uint32_t dataSize);

    void sendFile(string destIp, int32_t destPort, const string& filePath);

    void sendSegmentMetadata(const string& fileName, uintmax_t fileSize);

    int32_t recv(std::vector<uint8_t>& buffer, uint32_t length);

    void recvFile(void* buffer, uint32_t length);

    void receiveFileMetadata(string& fileName, uintmax_t& fileSize);

    string validateDataType(const std::string& metadata);
    
    void close();
};

struct FileMetadata {
    string fileName;
    uintmax_t fileSize;
    string fileType;
};

#endif