#include "header/segment.hpp"
#include <cstring>
#include <random>
#include <stdexcept>

/**
 * Creates a SYN segment with given sequence number
 * @param seqNum Initial sequence number
 * @return Segment with SYN flag and checksum
 */
Segment syn(uint32_t seqNum) {
    Segment segment = {};
    segment.seqNum = seqNum;
    segment.flags.syn = 1;
    segment.data_offset = 5; 
    segment.payload = nullptr;  
    return updateChecksum(segment);
}

/**
 * Creates an ACK segment with sequence and acknowledgment numbers
 * @param seqNum Sequence number
 * @param ackNum Acknowledgment number
 * @return Segment with ACK flag and checksum
 */
Segment ack(uint32_t seqNum, uint32_t ackNum) {
    Segment segment = {};
    segment.seqNum = seqNum;
    segment.ackNum = ackNum;
    segment.flags.ack = 1;
    segment.data_offset = 5;
    return updateChecksum(segment);
}

/**
 * Creates a SYN-ACK segment for handshake response
 * @param seqNum Sequence number
 * @return Segment with both SYN and ACK flags
 */
Segment synAck(uint32_t seqNum) {
    Segment segment = {};
    segment.seqNum = seqNum;
    segment.flags.syn = 1;
    segment.flags.ack = 1;
    segment.data_offset = 5;
    return updateChecksum(segment);
}

/**
 * Creates a FIN segment for connection termination
 * @return Segment with FIN flag
 */
Segment fin() {
    Segment segment = {};
    segment.flags.fin = 1;
    segment.data_offset = 5;
    return updateChecksum(segment);
}

/**
 * Creates a FIN-ACK segment for termination acknowledgment
 * @return Segment with both FIN and ACK flags
 */
Segment finAck() {
    Segment segment = {};
    segment.flags.fin = 1;
    segment.flags.ack = 1;
    segment.data_offset = 5;
    return updateChecksum(segment);
}

/**
 * Calculates checksum for segment validation
 * @param segment Segment to calculate checksum for
 * @return Two-byte checksum as array
 */
uint16_t calculateChecksum(Segment segment) {
    uint32_t sum = 0;
    
    // Calculate header checksum
    const uint8_t* bytePtr = reinterpret_cast<const uint8_t*>(&segment);
    const uint16_t* ptr = reinterpret_cast<const uint16_t*>(bytePtr);
    
    // Skip checksum field in calculation
    for (size_t i = 0; i < sizeof(Segment) / 2; i++) {
        if (i != 8) { // Skip checksum field
            sum += ptr[i];
        }
    }
    
    // Add payload if present
    if (segment.payload && segment.payloadSize > 0) {
        const uint16_t* payloadPtr = reinterpret_cast<const uint16_t*>(segment.payload);
        for (size_t i = 0; i < segment.payloadSize / 2; i++) {
            sum += payloadPtr[i];
        }
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

Segment updateChecksum(Segment segment) {
    segment.checksum = calculateChecksum(segment);  // Direct assignment
    return segment;
}

bool isValidChecksum(Segment segment) {
    uint16_t originalChecksum = segment.checksum;
    segment.checksum = 0;
    uint16_t calculatedChecksum = calculateChecksum(segment);
    return originalChecksum == calculatedChecksum;
}

uint32_t generateSecureSequenceNumber() {
    FILE* urandom = fopen("/dev/urandom", "r");
    if (!urandom) {
        throw std::runtime_error("Failed to open /dev/urandom");
    }
    uint32_t number;
    if (fread(&number, sizeof(number), 1, urandom) != 1) {
        fclose(urandom);
        throw std::runtime_error("Failed to read from /dev/urandom");
    }
    fclose(urandom);
    return number;
}