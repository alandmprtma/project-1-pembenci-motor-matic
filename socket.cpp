#include "header/socket.hpp"
#include "header/color.hpp"
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>

// helper methods
void TCPSocket::setTimeout(int seconds) {
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

bool TCPSocket::sendSegment(const Segment& segment, const struct sockaddr_in& addr) {
    // Send header first
    ssize_t headerSent = sendto(socket, &segment, sizeof(Segment), 0, 
                               (struct sockaddr*)&addr, sizeof(addr));
    
    if (headerSent < 0) {  // Add error check
        return false;
    }

    // Send payload if exists
    if (segment.payload && segment.payloadSize > 0) {
        usleep(1000);  // Small delay for reliability
        
        ssize_t payloadSent = sendto(socket, segment.payload, segment.payloadSize, 0,
                                    (struct sockaddr*)&addr, sizeof(addr));
                                    
        if (payloadSent < 0) {
            return false;
        }
    }
    
    return true;
}

int32_t TCPSocket::receiveSegment(Segment& segment, struct sockaddr_in& addr) {
    socklen_t addrLen = sizeof(addr);
    
    // Clear timeout
    struct timeval tv = {0, 0};
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    // Receive header
    int32_t headerBytes = recvfrom(socket, &segment, sizeof(Segment), 0,
                                 (struct sockaddr*)&addr, &addrLen);
    
    if (headerBytes <= 0) {
        return headerBytes;
    }

    if (peerAddrSet) {
        memcpy(&addr, &peerAddr, sizeof(struct sockaddr_in));
    }

    // If payload exists, receive it 
    if (segment.payloadSize > 0) {
        segment.payload = new uint8_t[segment.payloadSize];
        
        // Set timeout for payload
        tv.tv_sec = 1;
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
       
        int32_t payloadBytes = recvfrom(socket, segment.payload, segment.payloadSize, 0,
                                          (struct sockaddr*)&addr, &addrLen);
            
        if (payloadBytes > 0) {
            return headerBytes + payloadBytes;
        }

        delete[] segment.payload;
        segment.payload = nullptr;
        return -1;
    }
    
    return headerBytes;
}

TCPSocket::TCPSocket(string ip, int32_t port) : 
    ip(ip), 
    port(port),
    status(CLOSED),
    peerAddrSet(false) {  // Initialize peerAddrSet
    
    segmentHandler = new SegmentHandler();
    memset(&peerAddr, 0, sizeof(peerAddr));  // Initialize peerAddr
    
    socket = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket < 0) {
        delete segmentHandler;
        throw runtime_error("Socket creation failed");
    }
}

TCPSocket::~TCPSocket() {
    if (status != CLOSED) {
        close();  // Ensure proper connection closure
    }

    delete segmentHandler;
    ::close(socket);
}

struct sockaddr_in TCPSocket::createAddr(string ip, int32_t port) {
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (ip == "localhost") {
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // Force 127.0.0.1
    } else {
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
    }
    return addr;
}

bool TCPSocket::doHandshake(struct sockaddr_in& destAddr) {
    if (status == LISTEN) {
        Segment segment;
        while (status != ESTABLISHED) {
            int32_t bytes = receiveSegment(segment, destAddr);
            if (bytes <= 0) continue;

            if (segment.flags.syn) {
                peerAddr = destAddr;
                peerAddrSet = true;

                cout << Color::YELLOW << "[i]" << Color::RESET << " [Handshake] [S=" << segment.seqNum << "] Received SYN Request from " 
                     << inet_ntoa(peerAddr.sin_addr) << ":" << ntohs(peerAddr.sin_port) << endl;

                Segment synAck = syn(generateSecureSequenceNumber());
                synAck.flags.ack = 1;
                synAck.ackNum = segment.seqNum + 1;

                if (sendSegment(synAck, peerAddr)) {
                    status = SYN_RECEIVED;
                    cout << Color::YELLOW << "[i]" << Color::RESET << " [Handshake] [S=" << synAck.seqNum << "] [A=" << synAck.ackNum 
                         << "] Sending SYN-ACK Request to " << inet_ntoa(peerAddr.sin_addr) 
                         << ":" << ntohs(peerAddr.sin_port) << endl;
                }
            } else if (status == SYN_RECEIVED && segment.flags.ack) {
                cout << Color::GREEN << "[+]" << Color::RESET << " [Handshake] Received ACK Request from " 
                     << inet_ntoa(peerAddr.sin_addr) << ":" << ntohs(peerAddr.sin_port) << endl;
                status = ESTABLISHED;
            }
        }
    }
    
    else if (status == CLOSED) {
        peerAddr = destAddr; 
        peerAddrSet = true;
        
        //initiate handshake
        Segment synSegment = syn(generateSecureSequenceNumber());
        if (!sendSegment(synSegment, peerAddr)) {
            cout << Color::RED << "[!]" << Color::RESET << " Failed to send SYN" << endl;
            return false;
        }

        cout << Color::YELLOW << "[i]" << Color::RESET << " [Handshake] [S=" << synSegment.seqNum << "] Sending SYN request to " 
             << inet_ntoa(peerAddr.sin_addr) << ":" << ntohs(peerAddr.sin_port) << endl;
        status = SYN_SENT;

        // Wait for SYN-ACK and send ACK
        Segment segment;
        while (status != ESTABLISHED) {
            int32_t bytes = receiveSegment(segment, destAddr);
            if (bytes <= 0) continue;

            if (segment.flags.syn && segment.flags.ack) {
                cout << Color::GREEN << "[+]" << Color::RESET << " [Handshake] [S=" << segment.seqNum << "] [A=" << segment.ackNum 
                     << "] Received SYN-ACK Request from " << inet_ntoa(peerAddr.sin_addr) 
                     << ":" << ntohs(peerAddr.sin_port) << endl;

                Segment ack = ::ack(segment.ackNum, segment.seqNum + 1);
                if (sendSegment(ack, destAddr)) {
                    status = ESTABLISHED;
                    cout << Color::YELLOW << "[i]" << Color::RESET << " [Handshake] [A=" << segment.seqNum + 1 
                         << "] Sending ACK Request to " << inet_ntoa(peerAddr.sin_addr) 
                         << ":" << ntohs(peerAddr.sin_port) << endl;
                }
            }
        }
    }

    return status == ESTABLISHED;
}

void TCPSocket::listen() {
    struct sockaddr_in addr = createAddr(ip, port);
    if (bind(socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        throw runtime_error("Bind failed");
    }
    status = LISTEN;
}

void TCPSocket::send(string destIp, int32_t destPort, void* dataStream, uint32_t dataSize) { 
    if (!peerAddrSet) {
        cerr << Color::RED << "[!]" << Color::RESET << " No established connection" << endl;
        return;
    }
  
    segmentHandler->setDataStream((uint8_t*)dataStream, dataSize);
    cout << Color::YELLOW << "[i]" << Color::RESET << " Sending input to " << inet_ntoa(peerAddr.sin_addr) 
         << ":" << ntohs(peerAddr.sin_port) << endl;

    int helper = 0;
    while (status == ESTABLISHED) {
        Segment* segments = segmentHandler->advanceWindow(segmentHandler->getWindowSize(), helper);
        helper++;
        // Segment* segments = segmentHandler->advanceWindow(segmentHandler->segmentBuffer.size());
        if (!segments) {
            break;
        }

        // Get actual window size
        uint8_t actualWindowSize = segmentHandler->getWindowSize();

        // Send all segments first
        for (int i = 0; i < actualWindowSize; i++) {
            if (sendSegment(segments[i], peerAddr)) {
                cout << Color::YELLOW << "[i]" << Color::RESET << " [Established] [Seg " << i+1 
                     << "] [S=" << segments[i].seqNum << "] Sent" << endl;
            }
        }

        // Wait for ACKs
        cout << Color::MAGENTA << "[~]" << Color::RESET << " [Established] Waiting for segments to be ACKed" << endl;

        // Then handle ACKs
        for (int i = 0; i < actualWindowSize; i++) {
            Segment recvSegment;
            int32_t bytes = receiveSegment(recvSegment, peerAddr);

            if (bytes > 0) {
                // Check for FIN first
                if (recvSegment.flags.fin) {
                    cout << Color::YELLOW << "[i]" << Color::RESET << " [Closing] Received FIN request from "
                         << inet_ntoa(peerAddr.sin_addr) << ":"
                         << ntohs(peerAddr.sin_port) << endl;
                    
                    Segment finAckSegment = finAck();
                    if (sendSegment(finAckSegment, peerAddr)) {
                        cout << Color::YELLOW << "[i]" << Color::RESET << " [Closing] Sending FIN-ACK request to "
                             << inet_ntoa(peerAddr.sin_addr) << ":"
                             << ntohs(peerAddr.sin_port) << endl;
                        status = CLOSE_WAIT;
                        delete[] segments;
                        return;
                    }
                }

                // Handle normal ACK
                else if (isValidChecksum(recvSegment) && recvSegment.flags.ack) {
                    cout << Color::YELLOW << "[i]" << Color::RESET << " [Established] [Seg " << i+1 
                         << "] [A=" << recvSegment.ackNum << "] ACKed" << endl;
                    segmentHandler->handleAck(recvSegment.ackNum);
                }
            }
        }
        delete[] segments;
    }
}

int32_t TCPSocket::recv(std::vector<uint8_t>& buffer, uint32_t length) {
    if (status != ESTABLISHED && !doHandshake(peerAddr)) {
        // cerr << "status: " << status << endl;
        return -1;
    }

    cout << Color::YELLOW << "[i]" << Color::RESET << " Ready to receive input from " << inet_ntoa(peerAddr.sin_addr) 
         << ":" << ntohs(peerAddr.sin_port) << endl;
    cout << Color::MAGENTA << "[~]" << Color::RESET << " [Established] Waiting for segments to be sent" << endl;

    uint32_t totalReceived = 0;
    uint32_t segCount = 1;

    // Resize buffer to accommodate the incoming data
    buffer.resize(buffer.size() + length);

    while (true) {
        Segment segment;
        struct sockaddr_in tempAddr; 
        int32_t bytes = receiveSegment(segment, tempAddr);

        if (bytes > 0) {
            // Safe payload check
            if (segment.payloadSize > 0 && segment.payloadSize <= length - totalReceived) {
                cout << Color::YELLOW << "[i]" << Color::RESET << " [Established] [Seg " << segCount 
                     << "] [S=" << segment.seqNum << "] ACKed" << endl;

                // Copy the payload into the buffer
                std::copy(segment.payload, segment.payload + segment.payloadSize, 
                          buffer.begin() + totalReceived);

                totalReceived += segment.payloadSize;

                // Send ACK
                Segment ackSegment = ack(segment.seqNum + segment.payloadSize, 
                                         segment.seqNum);

                if (sendSegment(ackSegment, peerAddr)) {
                    cout << Color::YELLOW << "[i]" << Color::RESET << " [Established] [Seg " << segCount 
                         << "] [A=" << ackSegment.ackNum << "] Sent" << endl;
                }

                if (segment.payloadSize < 1460) {
                    break;
                }

                segCount++;
            }

            // Handle FIN after receiving all data
            else if (segment.flags.fin) {
                Segment finAckSegment = finAck();
                if (sendSegment(finAckSegment, peerAddr)) {
                    cout << Color::YELLOW << "[i]" << Color::RESET << " [Closing] Sending FIN-ACK request to "
                         << inet_ntoa(peerAddr.sin_addr) << ":"
                         << ntohs(peerAddr.sin_port) << endl;
                    status = LAST_ACK;
                }
                break;
            }
        } else {
            cerr << Color::RED << "[!]" << Color::RESET << " Error or connection closed masuk ke socket" << endl;
            break;
        }
    }

    // Resize the buffer to the actual received size
    buffer.resize(totalReceived);

    return totalReceived;
}

void TCPSocket::close() {
    struct timeval tv = {5, 0};
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    if (status == ESTABLISHED) {
        // Initiator closing sequence
        status = FIN_WAIT_1;
        
        Segment finSegment = fin();
        
        if (sendSegment(finSegment, peerAddr)) {
            cout << Color::YELLOW << "[i]" << Color::RESET << " [Closing] Sending FIN request to " 
                 << inet_ntoa(peerAddr.sin_addr) << ":" 
                 << ntohs(peerAddr.sin_port) << endl;

            time_t startTime = time(NULL);
            
            while (status == FIN_WAIT_1 && (time(NULL) - startTime < 5)) { 
                Segment finAckSegment;
                int32_t bytes = receiveSegment(finAckSegment, peerAddr);
                     
                if (bytes > 0) {
                    if (finAckSegment.flags.fin && finAckSegment.flags.ack) {
                        cout << Color::GREEN << "[+]" << Color::RESET << " [Closing] Received FIN-ACK request from "
                             << inet_ntoa(peerAddr.sin_addr) << ":" 
                             << ntohs(peerAddr.sin_port) << endl;
                        
                        status = TIME_WAIT;
                        
                        Segment ackSegment = ack(finAckSegment.seqNum + 1, 
                                               finAckSegment.seqNum);
                        if (sendSegment(ackSegment, peerAddr)) {
                            cout << Color::YELLOW << "[i]" << Color::RESET << " [Closing] Sending ACK request to "
                                 << inet_ntoa(peerAddr.sin_addr) << ":" 
                                 << ntohs(peerAddr.sin_port) << endl;
                            status = CLOSED;
                            break;
                        }
                    }
                }
            }
        } else {
            cout << "Failed to send FIN segment" << endl;
        }
    }
    else if (status == CLOSE_WAIT) {
        Segment ackSegment;
        time_t startTime = time(NULL);
        
        while (time(NULL) - startTime < 5) {
            int32_t bytes = receiveSegment(ackSegment, peerAddr);
            if (bytes > 0 && ackSegment.flags.ack) {
                cout << Color::GREEN << "[+]" << Color::RESET << " [Closing] Received final ACK from "
                     << inet_ntoa(peerAddr.sin_addr) << ":" 
                     << ntohs(peerAddr.sin_port) << endl;
                status = CLOSED;
                break;
            }
        }
    }
    
    cout << Color::YELLOW << "[i]" << Color::RESET << " Connection closed successfully" << endl;
    status = CLOSED;
    ::close(socket);
}