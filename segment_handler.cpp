#include "header/segment_handler.hpp"
#include "header/segment.hpp"
#include <iostream>
#include <cstring>
#include "header/color.hpp"
#include <vector>

const uint32_t SegmentHandler::MAX_SEGMENT_SIZE;
const uint32_t SegmentHandler::MAX_SEQ_NUM;
const time_t SegmentHandler::TIMEOUT_SECONDS;

SegmentHandler::SegmentHandler(uint8_t wSize) : 
    windowSize(wSize),
    currentSeqNum(0),
    currentAckNum(0),
    dataStream(nullptr),
    dataSize(0),
    dataIndex(0),
    LAR(0),
    LFS(0) {}

SegmentHandler::~SegmentHandler() {
    for (auto& segment : segmentBuffer) {
        if (segment.payload) {
            delete[] segment.payload;
            segment.payload = nullptr;
        }
    }
    segmentBuffer.clear();
}

void SegmentHandler::generateSegments() {
    const uint32_t MAX_PAYLOAD_SIZE = 1460; 

    if (!dataStream) {
        std::cerr << Color::RED << "[!]" << Color::RESET << " FATAL: dataStream is NULL" << std::endl;
        return;
    }

    int windowSizeInt = windowSize % 0xff;
    while (dataIndex < dataSize && segmentBuffer.size() < windowSizeInt) {

        uint8_t* byteStream = reinterpret_cast<uint8_t*>(dataStream);

        uint32_t remainingData = dataSize - dataIndex;
        uint32_t numSegments = (remainingData + MAX_PAYLOAD_SIZE - 1) / MAX_PAYLOAD_SIZE; // Pembulatan ke atas

        Segment* segments = new Segment[numSegments]; // Array untuk menyimpan segmen

        for (uint32_t i = 0; i < numSegments; ++i) {
            uint32_t currentChunkSize = std::min(MAX_PAYLOAD_SIZE, remainingData);
            segments[i].payload = new (std::nothrow) uint8_t[currentChunkSize];

            if (!segments[i].payload) {
                std::cerr << Color::RED << "[!]" << Color::RESET << " Failed to allocate memory for segment payload " << i << std::endl;
                for (uint32_t j = 0; j < i; ++j) delete[] segments[j].payload;
                delete[] segments;
                return;
            }

            segments[i].payloadSize = currentChunkSize;
            uint32_t startIdx = dataIndex;

            if (startIdx + currentChunkSize > dataSize) {
                std::cerr << Color::RED << "[!]" << Color::RESET << " FATAL: Attempt to access out of bounds memory" << std::endl;
                for (uint32_t j = 0; j <= i; ++j) delete[] segments[j].payload;
                delete[] segments;
                return;
            }

            try {
                std::memcpy(segments[i].payload, byteStream + startIdx, currentChunkSize);
            } catch (const std::exception& e) {
                std::cerr << Color::RED << "[!]" << Color::RESET << " Memory copy error for segment " << i << ": " << e.what() << std::endl;
                for (uint32_t j = 0; j <= i; ++j) delete[] segments[j].payload;
                delete[] segments;
                return;
            }

            segments[i].seqNum = currentSeqNum;

            try {
                segments[i] = updateChecksum(segments[i]);
            } catch (const std::exception& e) {
                std::cerr << Color::RED << "[!]" << Color::RESET << " Checksum calculation error for segment " << i << ": " << e.what() << std::endl;
                for (uint32_t j = 0; j <= i; ++j) delete[] segments[j].payload;
                delete[] segments;
                return;
            }

            segmentBuffer.push_back(segments[i]);

            currentSeqNum += currentChunkSize;
            dataIndex += currentChunkSize;
            remainingData -= currentChunkSize;
        }

        delete[] segments; // Hapus array segmen setelah selesai
    }
}

void SegmentHandler::setDataStream(uint8_t *dataStream, uint32_t dataSize) {
    if (dataStream == nullptr || dataSize == 0) {
        std::cerr << Color::RED << "[!]" << Color::RESET << " Invalid data stream or data size" << std::endl;
        return;
    }

    this->dataStream = dataStream;
    this->dataSize = dataSize;
    generateSegments();
}


uint8_t SegmentHandler::getWindowSize() {
    return windowSize;
}

Segment* SegmentHandler::advanceWindow(uint8_t sizeParam, int helper) {
    // int size = std::min(static_cast<int>(sizeParam), static_cast<int>(segmentBuffer.size()));
    int size = static_cast<int>(sizeParam);
    // if (size == static_cast<int>(segmentBuffer.size()-1)) {
    //     segmentBuffer.pop_back();
    // }
    // int size = static_cast<int>(sizeParam);
    // std::cout << "ukuran parameternya tuh " << size << std::endl;
    
    if (segmentBuffer.empty()) {
        std::cerr << Color::RED << "[!]" << Color::RESET << " Segment buffer is empty, cannot advance window" << std::endl;
        return nullptr;
    }

    // Create new window of segments
    Segment* segments = new Segment[size];
    uint32_t baseSeqNum = currentSeqNum;
    // Copy segments and update sequence numbers
    for (int i = 0; i < size; i++) {
        // if (advanceWindowProgress != 0){
        if (helper != 0) {
            segments[i] = segmentBuffer[1 + i];
        }
        else {
            segments[i] = segmentBuffer[i];
        }

        // }
        // std::cout << "isi payload: \n" << segments[i].payload << std::endl;
        // std::cout << "payload: " << segments[i].payload << std::endl;
        // segments[i].seqNum = currentSeqNum + (i * segmentBuffer[advanceWindowProgress +i].payloadSize);
    }
    // advanceWindowProgress++;
    // Update sequence number for next window
    // currentSeqNum = segments[size - 1].seqNum + segmentBuffer[size - 1].payloadSize;
    // std::cout << "current seqnum di advance: " << currentSeqNum << std::endl;
 
    return segments;
}

bool SegmentHandler::validateWindowSize() {
    return windowSize < (MAX_SEQ_NUM + 1)/2;
}

void SegmentHandler::handleAck(uint32_t ackNum) { 
    if (ackNum > LAR) {
        LAR = ackNum;
        while (!segmentBuffer.empty() && 
               segmentBuffer.front().seqNum < LAR) {
            segmentBuffer.erase(segmentBuffer.begin());
        }
        generateSegments();
    }
}

void SegmentHandler::checkTimeouts() { 
    time_t now = time(NULL);
    for (size_t i = 0; i < segmentTimers.size(); i++) {
        if (!segmentTimers[i].acknowledged && 
            (now - segmentTimers[i].sentTime) > TIMEOUT_SECONDS) {
            dataIndex = i * MAX_SEGMENT_SIZE;
            generateSegments();
            break;
        }
    }
}

bool SegmentHandler::isInWindow(uint32_t seqNum) {
    return (seqNum >= LAR && 
            seqNum < LAR + (windowSize * MAX_SEGMENT_SIZE));
}

void SegmentHandler::markAcknowledged(uint32_t seqNum) {
    size_t index = (seqNum - LAR) / MAX_SEGMENT_SIZE;
    if (index < segmentTimers.size()) {
        segmentTimers[index].acknowledged = true;
    }
}

void SegmentHandler::cleanupSegment(Segment& segment) {
    if (segment.payload) {
        delete[] segment.payload;
        segment.payload = nullptr;
    }
}