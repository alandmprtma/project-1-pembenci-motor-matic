#ifndef segment_handler_h
#define segment_handler_h

#include "segment.hpp"
#include <vector>
#include <ctime>

class SegmentHandler
{
private:
    uint8_t windowSize;
    uint32_t currentSeqNum;
    uint32_t currentAckNum;
    uint8_t *dataStream;  
    uint32_t dataSize;
    uint32_t dataIndex;
    
    uint32_t LAR;  // Last ACK Received
    uint32_t LFS;  // Last Frame Sent

    struct SegmentTimer {
        time_t sentTime;
        bool acknowledged;
    };

    static const uint32_t MAX_SEQ_NUM = 0xFFFFFFFF;
    bool validateWindowSize();
    std::vector<SegmentTimer> segmentTimers;

    static const time_t TIMEOUT_SECONDS = 5;
    void generateSegments();

public:
    std::vector<Segment> segmentBuffer; 
    static const uint32_t MAX_SEGMENT_SIZE = 1460;
    
    SegmentHandler(uint8_t wSize = 3); // defaultnya 3
    ~SegmentHandler();
    
    void setDataStream(uint8_t *dataStream, uint32_t dataSize);
    uint8_t getWindowSize();
    Segment *advanceWindow(uint8_t size, int helper);

    bool isInWindow(uint32_t seqNum);
    void handleAck(uint32_t ackNum);

    void checkTimeouts();
    void markAcknowledged(uint32_t seqNum);
    void cleanupSegment(Segment& segment);
};

#endif