#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <filesystem>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "header/socket.hpp"
#include "header/color.hpp" 
#include "header/node.hpp"    

void runSender(const std::string& host, int port);
void runReceiver(const std::string& host, int port);

int main(int argc, char* argv[]) {
    const string host = "0.0.0.0";
    int port;

    if (argc != 2) {
        cerr << "Usage: node [port]" << endl;
        return 1;
    }

    try {
        port = std::stoi(argv[1]);

        cout << Color::YELLOW << "[i]" << Color::RESET << " Node started at " << host << ":" << port << endl;
        cout << Color::CYAN << "[?]" << Color::RESET << " Please choose the operating mode" << endl;
        cout << Color::CYAN << "[?]" << Color::RESET << " 1. Sender" << endl;
        cout << Color::CYAN << "[?]" << Color::RESET << " 2. Receiver" << endl;
        cout << Color::CYAN << "[?]" << Color::RESET << " Input: ";
        
        int mode;
        std::cin >> mode;

        if (mode == 1) {
            cout << Color::GREEN << "[+]" << Color::RESET << " Node is now a sender" << endl;
            runSender(host, port);
        } else if (mode == 2) {
            cout << Color::GREEN << "[+]" << Color::RESET << " Node is now a receiver" << endl;
            runReceiver(host, port);
        } else {
            cerr << Color::RED << "[!]" << Color::RESET << " Invalid mode selected. Exiting." << endl;
        }

    } catch (const std::exception& e) {
        cerr << Color::RED << "[!]" << Color::RESET << " Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}

void runSender(const std::string& host, int port) {
    TCPSocket socket("0.0.0.0", port);

    // Get receiver's IP and port
    string receiverIP;
    cout << Color::CYAN << "[?]" << Color::RESET << " Enter receiver's IP address: ";
    cin >> receiverIP;

    int receiverPort;
    cout << Color::CYAN << "[?]" << Color::RESET << " Enter receiver's port: ";
    cin >> receiverPort;

    cout << Color::CYAN << "[?]" << Color::RESET << " Please choose the sending mode" << endl;
    cout << Color::CYAN << "[?]" << Color::RESET << " 1. User input" << endl;
    cout << Color::CYAN << "[?]" << Color::RESET << " 2. File input" << endl;
    cout << Color::CYAN << "[?]" << Color::RESET << " Input: ";
    
    int sendMode;
    std::cin >> sendMode;

    struct sockaddr_in destAddr = socket.createAddr(receiverIP, receiverPort);

    if (sendMode == 1) {
        // Text mode implementation
        cout << Color::CYAN << "[?]" << Color::RESET << " Input mode chosen, please enter your input: ";
        std::cin.ignore(); 
        std::string userInput;
        std::getline(std::cin, userInput);
        cout << Color::GREEN << "[+]" << Color::RESET << " User input has been successfully received." << endl;

        socket.listen();
        if (socket.doHandshake(destAddr)) {
            cout << Color::GREEN << "[+]" << Color::RESET << " Handshake completed. Sending data..." << endl;
            socket.send(receiverIP, receiverPort, userInput.data(), userInput.size());
            socket.close();
        } else {
            cerr << Color::RED << "[!]" << Color::RESET << " Handshake failed. Exiting..." << endl;
            return;
        }
    } 
    
    else if (sendMode == 2) {
        // File mode implementation

        cout << Color::CYAN << "[?]" << Color::RESET << " File mode chosen, please enter the file path: ";
        cin.ignore();
        string filePath;
        // string filePath = "socket.cpp";
        getline(cin, filePath);

        ifstream file(filePath, ios::binary);
        if (!file.is_open()) {
            cerr << Color::RED << "[!]" << Color::RESET << " Failed to open file: " << filePath << endl;
            return;
        }

        FileMetadata metadata;
        metadata.fileName = filesystem::path(filePath).filename().string();
        metadata.fileSize = filesystem::file_size(filePath);
        metadata.fileType = filesystem::path(filePath).extension().string();

        cout << Color::YELLOW << "[i]" << Color::RESET << " File name: " << metadata.fileName << endl;
        cout << Color::YELLOW << "[i]" << Color::RESET << " File size: " << metadata.fileSize << " bytes" << endl;
        cout << Color::YELLOW << "[i]" << Color::RESET << " File type: " << metadata.fileType << endl;

        string fileContent((istreambuf_iterator<char>(file)),
                          istreambuf_iterator<char>());
        file.close();

        const string DELIMITER = "<<META_END>>";

        string fullPayload =
            metadata.fileName + "\n" +
            to_string(metadata.fileSize) + "\n" +
            metadata.fileType + "\n" +
            DELIMITER +
            fileContent;

        cout << Color::GREEN << "[+]" << Color::RESET << " File has been successfully read." << endl;
        cout << Color::YELLOW << "[i]" << Color::RESET << " Listening for connection..." << endl;

        socket.listen();
        if (socket.doHandshake(destAddr)) {
            cout << Color::GREEN << "[+]" << Color::RESET << " Handshake completed. Sending file data..." << endl;
            socket.send(receiverIP, receiverPort, fullPayload.data(), fullPayload.size());
            socket.close();
        } else {
            cerr << Color::RED << "[!]" << Color::RESET << " Handshake failed. Exiting..." << endl;
            return;
        }
    } 
    else {
        cerr << Color::RED << "[!]" << Color::RESET << " Invalid mode selected. Exiting sender mode." << endl;
    }
} 


void runReceiver(const std::string& host, int port) {
    TCPSocket socket("0.0.0.0", port);

    // Get sender's IP and port
    string senderIP;
    cout << Color::CYAN << "[?]" << Color::RESET << " Enter the sender's IP address: ";
    cin >> senderIP;

    int serverPort;
    cout << Color::CYAN << "[?]" << Color::RESET << " Input the sender program's port: ";
    cin >> serverPort;

    cout << Color::GREEN << "[+]" << Color::RESET << " Trying to contact the sender at " << senderIP << ":" << serverPort << endl;

    struct sockaddr_in serverAddr = socket.createAddr(senderIP, serverPort);
    if (!socket.doHandshake(serverAddr)) {
        cout << Color::RED << "[!]" << Color::RESET << " Handshake failed" << endl;
        return;
    }

    const string DELIMITER = "<<META_END>>";

    vector<uint8_t> totalBuffer;
    vector<uint8_t> buffer;
    buffer.resize(1460*1460*1007); // Preallocate buffer size

    string receivedData;
    bool isFileTransfer = false;
    FileMetadata metadata;
    
    while (true) {
        int received = socket.recv(buffer, buffer.size());
        if (received >= 0) {
            // cout << "received: " << received << endl;
            // Append received data to total buffer
            size_t oldSize = totalBuffer.size();
            totalBuffer.resize(oldSize + received);
            std::copy(buffer.begin(), buffer.begin() + received, totalBuffer.begin() + oldSize);

            receivedData = string(reinterpret_cast<char*>(totalBuffer.data()), 
                            totalBuffer.size());
            
            if (!isFileTransfer) {
                size_t delimPos = receivedData.find(DELIMITER);
                if (delimPos != string::npos) {
                    isFileTransfer = true;
                    
                    // Parse metadata
                    string metadataStr = receivedData.substr(0, delimPos);
                    istringstream iss(metadataStr);
                    string line;
                    
                    getline(iss, metadata.fileName);
                    getline(iss, line); metadata.fileSize = stoull(line);
                    getline(iss, metadata.fileType);
                    
                    cout << Color::YELLOW << "[i]" << Color::RESET << " File transfer detected:" << endl
                        << "    Name: " << metadata.fileName << endl
                        << "    Size: " << metadata.fileSize << " bytes" << endl
                        << "    Type: " << metadata.fileType 
                        << "ukuran buffer saat ini:" << buffer.size() << endl;
                    cout << Color::YELLOW << "[i]" << Color::RESET << " File received" << endl;
                } else {
                    // Regular message
                    cout << Color::YELLOW << "[i]" << Color::RESET << " Received message: " << receivedData << endl;
                }
            }

            if (received < 1460) {
                // Last segment received
                if (isFileTransfer) {
                    // Extract file content after delimiter
                    size_t contentStart = receivedData.find(DELIMITER) + DELIMITER.length();
                    string fileContent = receivedData.substr(contentStart);
                    
                    // Save file
                    ofstream outFile("./test/" + metadata.fileName, ios::binary);
                    if (outFile) {
                        outFile.write(fileContent.data(), fileContent.length());
                        outFile.close();
                        cout << Color::GREEN << "[+]" << Color::RESET << " File saved successfully as: " << metadata.fileName << endl;
                    } else {
                        cerr << Color::RED << "[!]" << Color::RESET << " Failed to save file" << endl;
                    }
                }
                break;
            }
        } else {
            cerr << Color::RED << "[!]" << Color::RESET << " Error or connection closed masuk ke main dgn nilai received " << received  << " dan ukuran buffer " << buffer.size() << endl;
            break;
        }
    }

    socket.close();
}
