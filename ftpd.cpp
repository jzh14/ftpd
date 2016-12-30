#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <algorithm>
#include <ifaddrs.h>
#include <fstream>
#include <queue>
#include <sys/ioctl.h>
#include "messages.h"
#include "utils.h"
#include "lib_INIReader.h"
#include "list.h"
#include "network.h"
using namespace std;

static const char* SERVER_PORT = "21";   // port we're listening on
static const char* DATA_PORT = "20";
static const int FTP_ADDR_LENGTH = 28;
static const int MAX_PACKET_RATE = 5;

enum STATUS {
    WAITING_FOR_USER = 1, WAITING_FOR_PASSWORD = 2, LOGGED_IN = 3,
    PASV_WAITING = 4, PASV_SENDING = 5, PASV_RECEIVING = 6,
    PORT_WAITING = 7, PORT_SENDING = 8, PORT_RECEIVING = 9
};

enum MODE {
    ASCII = 1, IMAGE = 2
};

int main(int argc, char **argv) {
    if (argc != 2) {
        cout << "Usage: " << argv[0] << " <config file>" << endl;
        return 1;
    }
    INIReader inputFile(argv[1]);
    if (inputFile.ParseError() < 0) {
        cout << "Input file error" << endl;
        return 1;
    }

    // <config>
    bool config_allow_anonymous = inputFile.GetInteger("general", "anonymous_enable", false);
    string welcome_message = inputFile.Get("general", "welcome_message", "");
    // </config>

    // <status>
    map<int, string> username;
    map<int, STATUS> status;
    map<int, int> pasvSocket; // for PASV mode
    map<int, int> portSocket; // for PORT mode
    map<int, string> path;
    map<int, string> rootPath;
    map<int, int> reverseSocketTable;
    map<int, MODE> mode;
    map<int, bool> pasvPending;
    map<int, string> pendingData;
    map<int, string> storPath;
    map<int, ofstream*> outFile;
    map<int, int> pasvPendingType;
    map<int, string> pasvBuffer;
    map<int, string> pendingRename;
    // </status>

    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    fd_set pasv_fds;
    fd_set pasv_data_fds;
    fd_set port_data_fds;
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[1024];    // buffer for client data
    int nbytes;
    queue<string> commandQueue;

    char remoteIP[INET6_ADDRSTRLEN];

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);
    FD_ZERO(&pasv_fds);
    FD_ZERO(&pasv_data_fds);
    FD_ZERO(&port_data_fds);

    if ((listener = bindSocket(SERVER_PORT)) < 0) {
        exit(listener);
    }

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    while (true) {
        read_fds = master; // copy it
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        printf("ftpd: new connection from %s on socket %d\n",
                               inet_ntop(remoteaddr.ss_family,
                                         get_in_addr((struct sockaddr*)&remoteaddr),
                                         remoteIP, INET6_ADDRSTRLEN),
                               newfd);
                        status[newfd] = WAITING_FOR_USER;
                        //sendall(newfd, MESSAGE_READY, strlen(MESSAGE_READY), 0);
                        string message(MESSAGE_GREETING);
                        message.append(welcome_message);
                        message.append("\r\n");
                        sendall(newfd, message.c_str(), message.size(), 0);
                    }
                }
                else if (FD_ISSET(i, &pasv_fds)) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(i, (struct sockaddr *)&remoteaddr, &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        //cout << "i = " << i << ", newfd = " << newfd << ", reverseSocketTable[i] = " <<
                        //     reverseSocketTable[i] << endl;
                        FD_CLR(i, &pasv_fds);
                        FD_CLR(i, &master);
                        FD_SET(newfd, &pasv_data_fds); // add to pasv_data_fds set
                        FD_SET(newfd, &master);
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        //close(i);
                        reverseSocketTable[newfd] = reverseSocketTable[i];
                        pasvSocket[reverseSocketTable[i]] = newfd;
                        if (pasvPending[reverseSocketTable[i]]) {
                            if (pasvPendingType[reverseSocketTable[i]] == 1) { // Waiting to send
                                if (!fork()) {
                                    sendall(newfd, pendingData[reverseSocketTable[i]].c_str(),
                                            pendingData[reverseSocketTable[i]].size(), 0);
                                    sendall(reverseSocketTable[i], MESSAGE_TRANSFER_COMPLETE,
                                            strlen(MESSAGE_TRANSFER_COMPLETE), 0);
                                    close(newfd);
                                    exit(0);
                                }
                                if (status[reverseSocketTable[i]] == PASV_SENDING) {
                                    status[reverseSocketTable[i]] = PASV_WAITING;
                                }
                                FD_CLR(newfd, &master);
                                FD_CLR(newfd, &pasv_data_fds);
                                close(newfd);
                                pasvPending[reverseSocketTable[i]] = false;
                            }
                            else if (pasvPendingType[reverseSocketTable[i]] == 2) { // Waiting to receive
                                //cout << "PASV connection established for socket " << reverseSocketTable[i] << endl;
                                /*
                                cout << "Writing " << pasvBuffer[reverseSocketTable[i]] << " bytes" << endl;
                                outFile[i]->write(pasvBuffer[reverseSocketTable[i]].c_str(),
                                                 pasvBuffer[reverseSocketTable[i]].size());
                                outFile[i]->close();
                                close(newfd);
                                */
                                sendall(reverseSocketTable[i], MESSAGE_STOR_OK, strlen(MESSAGE_STOR_OK), 0);
                                pasvPending[reverseSocketTable[i]] = false;
                            }
                        }
                    }
                }
                else if (FD_ISSET(i, &pasv_data_fds) || FD_ISSET(i, &port_data_fds)) {
                    int sockfd = reverseSocketTable[i];
                    //cout << "sockfd : " << sockfd << ", i : " << i << endl;
                    char buffer[1 << 16]; // 64KB buffer
                    nbytes = recv(i, buffer, sizeof(buffer), 0);
                    if (status[sockfd] != PASV_RECEIVING && FD_ISSET(i, &pasv_data_fds)) {
                        //cout << "Ah... Something strange..." << endl;
                        if (pasvBuffer.count(sockfd) == 0) {
                            pasvBuffer[sockfd] = string("");
                        }
                        if (nbytes > 0) {
                            pasvBuffer[sockfd].append(buffer, nbytes);
                        }
                        else {
                            FD_CLR(i, &pasv_data_fds);
                            FD_CLR(i, &port_data_fds);
                            FD_CLR(i, &master);
                            sendall(sockfd, MESSAGE_STOR_COMPLETE, strlen(MESSAGE_STOR_COMPLETE), 0);
                            /*
                            outFile[sockfd]->write(pasvBuffer[sockfd].c_str(),
                                              pasvBuffer[sockfd].size());
                            outFile[sockfd]->close();
                            */
                            close(i);
                        }
                    }
                    else if (status[sockfd] == PASV_RECEIVING && FD_ISSET(i, &pasv_data_fds) ||
                             //if (FD_ISSET(i, &pasv_data_fds) ||
                             status[sockfd] == PORT_RECEIVING && FD_ISSET(i, &port_data_fds)) {

                        if (nbytes > 0) {
                            outFile[sockfd]->write(buffer, nbytes);
                        }
                        else {
                            outFile[sockfd]->close();
                            FD_CLR(i, &pasv_data_fds);
                            FD_CLR(i, &port_data_fds);
                            FD_CLR(i, &master);
                            sendall(sockfd, MESSAGE_STOR_COMPLETE, strlen(MESSAGE_STOR_COMPLETE), 0);
                            status[sockfd] = ((status[sockfd] == PASV_RECEIVING) ? PASV_WAITING : PORT_WAITING);
                            close(i);
                        }
                    }
                }
                else {
                    // handle data from a client
                    string bufstr;
                    nbytes = recv(i, buf, sizeof buf, 0);
                    if (nbytes <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("ftpd: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    }
                    else {
                        memset(buf + nbytes, 0, sizeof(buf) - nbytes);
                        bufstr.append(buf);
                        vector<string> bufLines = splitString(bufstr, string("\n"));
                        string bufLine;
                        int count = 0;
                        for (vector<string>::iterator it = bufLines.begin(); it != bufLines.end(); ++it) {
                            bufLine = *it;
                            while (bufLine.back() == '\r' || bufLine.back() == '\n') {
                                bufLine.pop_back();
                            }
                            commandQueue.push(bufLine);
                            if (++count > MAX_PACKET_RATE) {
                                break;
                            }
                            //cout << "Push: " << bufLine << endl;
                        }
                    }
                    while (commandQueue.size()) {
                        // we got some data from a client
                        bufstr = commandQueue.front();
                        commandQueue.pop();
                        //cout << "Pop: " << bufstr << endl;
                        vector<string> args = splitString(bufstr, string(" "));
                        if (args.size() == 0) {
                            // Do nothing
                        }
                        else if (strToUpper(args[0]).compare("OPTS") == 0) {
                            if (args.size() > 1 && strToUpper(args[1]).compare("UTF8") == 0) {
                                sendall(i, MESSAGE_UTF8, strlen(MESSAGE_UTF8), 0);
                            }
                        }
                        else if (strToUpper(args[0]).compare("USER") == 0) {
                            string inputUsername;
                            if (args.size() > 1) {
                                for (vector<string>::iterator it = args.begin() + 1; it != args.end(); ++it) {
                                    inputUsername.append(*it);
                                    inputUsername.append(" ");
                                }
                                inputUsername.pop_back();
                                username[i] = inputUsername;
                                status[i] = WAITING_FOR_PASSWORD;
                                sendall(i, MESSAGE_SPECIFY_PASSWORD, strlen(MESSAGE_SPECIFY_PASSWORD), 0);
                            }
                            else {
                                sendall(i, MESSAGE_NO_USERNAME_OR_PASSWORD, strlen(MESSAGE_NO_USERNAME_OR_PASSWORD), 0);
                            }
                        }
                        else if (strToUpper(args[0]).compare("PASS") == 0) {
                            string inputPassword;
                            if (args.size() > 1) {
                                for (vector<string>::iterator it = args.begin() + 1; it != args.end(); ++it) {
                                    inputPassword.append(*it);
                                    inputPassword.append(" ");
                                }
                                inputPassword.pop_back();
                            }
                            else {
                                inputPassword.clear();
                            }
                            string correctPassword = inputFile.Get("login", username[i], "");
                            if (username[i].compare("anonymous") == 0 || correctPassword.compare(inputPassword) == 0) {
                                status[i] = LOGGED_IN;
                                sendall(i, MESSAGE_LOGIN_SUCCESSFUL, strlen(MESSAGE_LOGIN_SUCCESSFUL), 0);
                                rootPath[i] = inputFile.Get("path", username[i], "");
                                if (rootPath[i].back() == '/') {
                                    rootPath[i].pop_back();
                                }
                                path[i] = "/";
                            }
                            else {
                                sendall(i, MESSAGE_LOGIN_INCORRECT, strlen(MESSAGE_LOGIN_INCORRECT), 0);
                            }
                        }
                        else if (strToUpper(args[0]).compare("QUIT") == 0) {
                            sendall(i, MESSAGE_QUIT, strlen(MESSAGE_QUIT), 0);
                        }
                        else if (strToUpper(args[0]).compare("PORT") == 0) {
                            if (args.size() != 2) {
                                sendall(i, MESSAGE_ILLEGAL_PORT, strlen(MESSAGE_ILLEGAL_PORT), 0);
                            }
                            else {
                                vector<string> addrParts = splitString(args[1], ",");
                                if (addrParts.size() != 6) {
                                    sendall(i, MESSAGE_ILLEGAL_PORT, strlen(MESSAGE_ILLEGAL_PORT), 0);
                                }
                                else {
                                    unsigned int temp[2];
                                    char portNum[PORT_LENGTH];
                                    sscanf(addrParts[4].c_str(), "%u", &temp[0]);
                                    sscanf(addrParts[5].c_str(), "%u", &temp[1]);
                                    sprintf(portNum, "%u", (temp[0] << 8) + temp[1]);
                                    string ipAddr(addrParts[0]);
                                    ipAddr.append(".");
                                    ipAddr.append(addrParts[1]);
                                    ipAddr.append(".");
                                    ipAddr.append(addrParts[2]);
                                    ipAddr.append(".");
                                    ipAddr.append(addrParts[3]);
                                    struct addrinfo* ai;
                                    if (getAddr(portNum, ai, ipAddr.c_str()) < 0) {
                                        sendall(i, MESSAGE_ILLEGAL_PORT, strlen(MESSAGE_ILLEGAL_PORT), 0);
                                    }
                                    else {
                                        for (struct addrinfo *p = ai; p != NULL; p = p->ai_next) {
                                            int connector = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                                            //int connector = bindSocket("20");
                                            if (connector < 0) {
                                                continue;
                                            }
                                            if (connect(connector, p->ai_addr, p->ai_addrlen) == 0) {
                                                portSocket[i] = connector;
                                                status[i] = PORT_WAITING;
                                                reverseSocketTable[connector] = i;
                                                FD_SET(connector, &master);
                                                FD_SET(connector, &port_data_fds);
                                                if (connector > fdmax) {
                                                    fdmax = connector;
                                                }
                                                sendall(i, MESSAGE_PORT_SUCCESSFUL, strlen(MESSAGE_PORT_SUCCESSFUL), 0);
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        else if (strToUpper(args[0]).compare("PASV") == 0) {
                            if (status[i] <= 2) { // not logged in
                                sendall(i, MESSAGE_NO_USERNAME_OR_PASSWORD, strlen(MESSAGE_NO_USERNAME_OR_PASSWORD), 0);
                            }
                            else {
                                pasvSocket[i] = bindSocket("0");
                                struct sockaddr addr;
                                socklen_t addrlen = sizeof(addr);
                                getsockname(pasvSocket[i], &addr, &addrlen);
                                unsigned short portNum = ntohs(*(unsigned short *) (addr.sa_data + 0));
                                addrlen = sizeof(addr);
                                int result = getsockname(listener, &addr, &addrlen);
                                string message(MESSAGE_PASV_BEGIN);
                                message.reserve(message.size() + FTP_ADDR_LENGTH);
                                char str[INET_ADDRSTRLEN];
                                unsigned long ipNum = ntohl(getPublicIP());
                                char buffer[INET_ADDRSTRLEN + PORT_LENGTH + strlen(MESSAGE_PASV_END)];
                                sprintf(buffer, "%lu,%lu,%lu,%lu,%u,%u%s", (ipNum & (255 << 24)) >> 24,
                                        (ipNum & (255 << 16)) >> 16, (ipNum & (255 << 8)) >> 8, ipNum & 255,
                                        (portNum & (255 << 8)) >> 8, portNum & 255, MESSAGE_PASV_END);
                                message.append(buffer);
                                reverseSocketTable[pasvSocket[i]] = i;
                                if (listen(pasvSocket[i], 1)) {
                                    perror("listen");
                                    exit(3);
                                }
                                FD_SET(pasvSocket[i], &pasv_fds);
                                FD_SET(pasvSocket[i], &master);
                                if (pasvSocket[i] > fdmax) {
                                    fdmax = pasvSocket[i];
                                }
                                status[i] = PASV_WAITING;
                                pasvPending[i] = true;
                                /*
                                string message(MESSAGE_PASV_BEGIN);
                                unsigned long ipNum = ntohl(getPublicIP());
                                char buffer[INET_ADDRSTRLEN + PORT_LENGTH + strlen(MESSAGE_PASV_END)];
                                sprintf(buffer, "%lu,%lu,%lu,%lu,20%s", (ipNum & (255 << 24)) >> 24,
                                        (ipNum & (255 << 16)) >> 16, (ipNum & (255 << 8)) >> 8,
                                        ipNum & 255, MESSAGE_PASV_END);
                                addrlen = sizeof(remoteaddr);
                                newfd = accept(data_listener, (struct sockaddr *)&remoteaddr, &addrlen);
                                FD_SET(newfd, &pasv_fds);
                                if (newfd > fdmax) {
                                    fdmax = newfd;
                                }
                                status[i] = PASV_WAITING;
                                pasvSocket[i] = newfd;
                                */
                                sendall(i, message.c_str(), message.size(), 0);
                            }
                        }
                        else if (strToUpper(args[0]).compare("XPWD") == 0 || strToUpper(args[0]).compare("PWD") == 0) {
                            if (status[i] <= 2) { // not logged in
                                sendall(i, MESSAGE_NO_USERNAME_OR_PASSWORD, strlen(MESSAGE_NO_USERNAME_OR_PASSWORD), 0);
                            }
                            else {
                                //cout << path[i] << endl;
                                string message(MESSAGE_PWD_BEGIN);
                                message.append(path[i]);
                                message.append(MESSAGE_PWD_END);
                                sendall(i, message.c_str(), message.size(), 0);
                            }
                        }
                        else if (strToUpper(args[0]).compare("NLST") == 0 ||
                                 strToUpper(args[0]).compare("LIST") == 0) {
                            if (status[i] <= 2) {
                                sendall(i, MESSAGE_NO_USERNAME_OR_PASSWORD, strlen(MESSAGE_NO_USERNAME_OR_PASSWORD), 0);
                            }
                            else if (status[i] == 3) {
                                sendall(i, MESSAGE_SELECT_MODE_FIRST, strlen(MESSAGE_SELECT_MODE_FIRST), 0);
                            }
                            else if (status[i] > 3 && status[i] < 10) {
                                string message;
                                string filePath;
                                if (bufstr.size() > 5) {
                                    filePath = bufstr.substr(5);
                                }
                                else {
                                    filePath = "/";
                                }
                                vector<string> pathParts = splitString(filePath, string("/"));
                                string tempPath = path[i];
                                if (pathParts.size() > 1) {
                                    for (int i = 0; i < pathParts.size() - 1; ++i) {
                                        tempPath = chDir(tempPath, pathParts[i]);
                                    }
                                }
                                string fullPath(rootPath[i] + tempPath + (pathParts.size() ? pathParts[pathParts.size() - 1] : ""));
                                if (fileExists(fullPath.c_str())) {
                                    if (strToUpper(args[0]).compare("NLST") == 0) {
                                        vector <string> files = listDir(fullPath);
                                        for (vector<string>::iterator it = files.begin(); it != files.end(); ++it) {
                                            message.append(*it);
                                            message.append("\r\n");
                                        }
                                    }
                                    else {
                                        message = listPath((fullPath).c_str());
                                    }
                                }
                                sendall(i, MESSAGE_DIR_BEGIN, strlen(MESSAGE_DIR_BEGIN), 0);
                                if (status[i] > 6 && status[i] < 10) {
                                    if (!fork()) {
                                        sendall(portSocket[i], message.c_str(), message.size(), 0);
                                        close(portSocket[i]);
                                        exit(0);
                                    }
                                    FD_CLR(portSocket[i], &master);
                                    FD_CLR(portSocket[i], &port_data_fds);
                                    close(portSocket[i]);
                                }
                                else {
                                    if (!pasvPending[i]) {
                                        if (!fork()) {
                                            sendall(pasvSocket[i], message.c_str(), message.size(), 0);
                                            close(pasvSocket[i]);
                                            exit(0);
                                        }
                                        FD_CLR(pasvSocket[i], &master);
                                        FD_CLR(pasvSocket[i], &pasv_data_fds);
                                        close(pasvSocket[i]);
                                    }
                                    else {
                                        pendingData[i] = message;
                                        pasvPendingType[i] = 1; // Waiting to send;
                                    }
                                }
                                sendall(i, MESSAGE_DIR_END, strlen(MESSAGE_DIR_END), 0);
                            }
                        }
                        else if (strToUpper(args[0]).compare("CWD") == 0) {
                            if (status[i] <= 2) {
                                sendall(i, MESSAGE_NO_USERNAME_OR_PASSWORD, strlen(MESSAGE_NO_USERNAME_OR_PASSWORD), 0);
                            }
                            else {
                                string filePath;
                                if (bufstr.size() > 4) {
                                    filePath = bufstr.substr(4);
                                }
                                else {
                                    filePath = "";
                                }
                                pair<string, string> pathPair = generatePath("", path[i], filePath);
                                //cout << pathPair.first << ", " << pathPair.second << endl;
                                if (isDir((rootPath[i] + pathPair.first + pathPair.second).c_str())) {
                                    path[i] = pathPair.first + pathPair.second;
                                    while (path[i].back() == '/') {
                                        path[i].pop_back();
                                    }
                                    path[i].append("/");
                                }
                                else {
                                    sendall(i, MESSAGE_CD_FAILED, strlen(MESSAGE_CD_FAILED), 0);
                                    break;
                                }
                                sendall(i, MESSAGE_CD_SUCCESS, strlen(MESSAGE_CD_SUCCESS), 0);
                            }
                        }
                        else if (strToUpper(args[0]).compare("HELP") == 0) {
                            string message(MESSAGE_HELP_BEGIN);
                            message.append(MESSAGE_HELP);
                            message.append(MESSAGE_HELP_END);
                            sendall(i, message.c_str(), message.size(), 0);
                        }
                        else if (strToUpper(args[0]).compare("NOOP") == 0) {
                            sendall(i, MESSAGE_NOOP_OK, strlen(MESSAGE_NOOP_OK), 0);
                        }
                        else if (strToUpper(args[0]).compare("SYST") == 0) {
                            sendall(i, MESSAGE_SYSTEM_TYPE, strlen(MESSAGE_SYSTEM_TYPE), 0);
                        }
                        else if (strToUpper(args[0]).compare("TYPE") == 0) {
                            if (args.size() != 2) {
                                sendall(i, MESSAGE_UNRECOGNISED_MODE, strlen(MESSAGE_UNRECOGNISED_MODE), 0);
                            }
                            else if (strToUpper(args[1]).compare("I") == 0) {
                                sendall(i, MESSAGE_BINARY_MODE, strlen(MESSAGE_BINARY_MODE), 0);
                                mode[i] = IMAGE;
                            }
                            else if (strToUpper(args[1]).compare("A") == 0) {
                                sendall(i, MESSAGE_ASCII_MODE, strlen(MESSAGE_ASCII_MODE), 0);
                                mode[i] = ASCII;
                            }
                            else {
                                sendall(i, MESSAGE_UNRECOGNISED_MODE, strlen(MESSAGE_UNRECOGNISED_MODE), 0);
                            }
                        }
                        else if (strToUpper(args[0]).compare("RETR") == 0) {
                            if (status[i] == PORT_WAITING || status[i] == PASV_WAITING) {
                                pair<string, string> pathPair;
                                if (bufstr.size() > 5) {
                                    pathPair = generatePath(rootPath[i], path[i], bufstr.substr(5));
                                    if (isFile((pathPair.first + pathPair.second).c_str())) {
                                        ifstream fin(pathPair.first + pathPair.second, ios::in|ios::binary|ios::ate);
                                        streampos size = fin.tellg();
                                        char *memblock = new char[size];
                                        fin.seekg(0, ios::beg);
                                        fin.read(memblock, (int)size);
                                        fin.close();
                                        string message(MESSAGE_SEND_PART1);
                                        message.append(pathPair.second);
                                        message.append(MESSAGE_SEND_PART2);
                                        message.append(to_string(size));
                                        message.append(MESSAGE_SEND_PART3);
                                        sendall(i, message.c_str(), message.size(), 0);
                                        if (status[i] == PORT_WAITING) {
                                            status[i] = PORT_SENDING;
                                            if (!fork()) {
                                                sendall(portSocket[i], memblock, (int)size, 0);
                                                sendall(i, MESSAGE_TRANSFER_COMPLETE, strlen(MESSAGE_TRANSFER_COMPLETE), 0);
                                                close(portSocket[i]);
                                                exit(0);
                                            }
                                            FD_CLR(portSocket[i], &master);
                                            FD_CLR(portSocket[i], &port_data_fds);
                                            close(portSocket[i]);
                                            status[i] = PORT_WAITING;
                                        }
                                        else {
                                            if (!pasvPending[i]) {
                                                status[i] = PASV_SENDING;
                                                if (!fork()) {
                                                    sendall(pasvSocket[i], memblock, (int)size, 0);
                                                    sendall(i, MESSAGE_TRANSFER_COMPLETE,
                                                            strlen(MESSAGE_TRANSFER_COMPLETE), 0);
                                                    close(pasvSocket[i]);
                                                    exit(0);
                                                }
                                                FD_CLR(pasvSocket[i], &master);
                                                FD_CLR(pasvSocket[i], &pasv_data_fds);
                                                close(pasvSocket[i]);
                                                status[i] = PASV_WAITING;
                                            }
                                            else {
                                                status[i] = PASV_SENDING;
                                                pendingData[i] = string(memblock, (int)size);
                                                pasvPendingType[i] = 1; // Waiting to send
                                            }
                                        }
                                        delete[] memblock;
                                    }
                                    else {
                                        sendall(i, MESSAGE_FAIL_OPEN_FILE, strlen(MESSAGE_FAIL_OPEN_FILE), 0);
                                    }
                                }
                                else {
                                    sendall(i, MESSAGE_FAIL_OPEN_FILE, strlen(MESSAGE_FAIL_OPEN_FILE), 0);
                                }
                            }
                            else {
                                sendall(i, MESSAGE_SELECT_MODE_FIRST, strlen(MESSAGE_SELECT_MODE_FIRST), 0);
                            }
                        }
                        else if (strToUpper(args[0]).compare("STOR") == 0 || strToUpper(args[0]).compare("APPE") == 0) {
                            if (status[i] == PORT_WAITING || status[i] == PASV_WAITING) {
                                pair<string, string> pathPair;
                                if (bufstr.size() > 5) {
                                    pathPair = generatePath(rootPath[i], path[i], bufstr.substr(5));
                                    storPath[i] = pathPair.first + pathPair.second;
                                    outFile[i] = new ofstream(storPath[i], (strToUpper(args[0]).compare("APPE") == 0)
                                                                           ? ios::out|ios::binary|ios::app
                                                                           : ios::out|ios::binary);
                                    if (pasvPending[i]) {
                                        pasvPendingType[i] = 2; // Waiting to receive
                                    }

                                    if (status[i] == PORT_WAITING) {
                                        status[i] = PORT_RECEIVING;
                                    }
                                    else if (status[i] == PASV_WAITING) {
                                        status[i] = PASV_RECEIVING;
                                    }
                                }
                                else {
                                    sendall(i, MESSAGE_FAIL_CREATE_FILE, strlen(MESSAGE_FAIL_CREATE_FILE), 0);
                                }
                            }
                        }
                        else if (strToUpper(args[0]).compare("DELE") == 0) {
                            if (status[i] <= 2) { // not logged in
                                sendall(i, MESSAGE_NO_USERNAME_OR_PASSWORD, strlen(MESSAGE_NO_USERNAME_OR_PASSWORD), 0);
                            }
                            else {
                                if (bufstr.size() > 5) {
                                    pair <string, string> pathPair = generatePath(rootPath[i], path[i],
                                                                                  bufstr.substr(5));
                                    if (remove((pathPair.first + pathPair.second).c_str())) {
                                        sendall(i, MESSAGE_DELE_FAIL, strlen(MESSAGE_DELE_FAIL), 0);
                                    } else {
                                        sendall(i, MESSAGE_DELE_SUCCESS, strlen(MESSAGE_DELE_SUCCESS), 0);
                                    }
                                } else {
                                    sendall(i, MESSAGE_DELE_FAIL, strlen(MESSAGE_DELE_FAIL), 0);
                                }
                            }
                        }
                        else if (strToUpper(args[0]).compare("MKD") == 0) {
                            if (status[i] <= 2) { // not logged in
                                sendall(i, MESSAGE_NO_USERNAME_OR_PASSWORD, strlen(MESSAGE_NO_USERNAME_OR_PASSWORD), 0);
                            }
                            else {
                                if (bufstr.size() > 4) {
                                    pair <string, string> pathPair = generatePath(rootPath[i], path[i],
                                                                                  bufstr.substr(4));
                                    if (mkdir((pathPair.first + pathPair.second).c_str(), 0777)) {
                                        sendall(i, MESSAGE_MKDIR_FAIL, strlen(MESSAGE_MKDIR_FAIL), 0);
                                    } else {
                                        string message(MESSAGE_MKDIR_SUCCESS_BEGIN);
                                        message.append(bufstr.substr(4));
                                        message.append(MESSAGE_MKDIR_SUCCESS_END);
                                        sendall(i, message.c_str(), message.size(), 0);
                                    }
                                } else {
                                    sendall(i, MESSAGE_MKDIR_FAIL, strlen(MESSAGE_MKDIR_FAIL), 0);
                                }
                            }
                        }
                        else if (strToUpper(args[0]).compare("RMD") == 0) {
                            if (status[i] <= 2) { // not logged in
                                sendall(i, MESSAGE_NO_USERNAME_OR_PASSWORD, strlen(MESSAGE_NO_USERNAME_OR_PASSWORD), 0);
                            }
                            else {
                                if (bufstr.size() > 4) {
                                    pair <string, string> pathPair = generatePath(rootPath[i], path[i],
                                                                                  bufstr.substr(4));
                                    if (rmdir((pathPair.first + pathPair.second).c_str())) {
                                        sendall(i, MESSAGE_RMDIR_FAIL, strlen(MESSAGE_RMDIR_FAIL), 0);
                                    } else {
                                        sendall(i, MESSAGE_RMDIR_SUCCESS, strlen(MESSAGE_RMDIR_SUCCESS), 0);
                                    }
                                } else {
                                    sendall(i, MESSAGE_RMDIR_FAIL, strlen(MESSAGE_RMDIR_FAIL), 0);
                                }
                            }
                        }
                        else if (strToUpper(args[0]).compare("RNFR") == 0) {
                            if (status[i] <= 2) { // not logged in
                                sendall(i, MESSAGE_NO_USERNAME_OR_PASSWORD, strlen(MESSAGE_NO_USERNAME_OR_PASSWORD), 0);
                            }
                            else {
                                if (bufstr.size() > 5) {
                                    pair <string, string> pathPair = generatePath(rootPath[i], path[i],
                                                                                  bufstr.substr(5));
                                    pendingRename[i] = pathPair.first + pathPair.second;
                                    sendall(i, MESSAGE_RNFR_SUCCESS, strlen(MESSAGE_RNFR_SUCCESS), 0);
                                } else {
                                    sendall(i, MESSAGE_RNFR_FAIL, strlen(MESSAGE_RNFR_FAIL), 0);
                                }
                            }
                        }
                        else if (strToUpper(args[0]).compare("RNTO") == 0) {
                            if (status[i] <= 2) { // not logged in
                                sendall(i, MESSAGE_NO_USERNAME_OR_PASSWORD, strlen(MESSAGE_NO_USERNAME_OR_PASSWORD), 0);
                            }
                            else {
                                if (bufstr.size() > 5) {
                                    pair <string, string> pathPair = generatePath(rootPath[i], path[i],
                                                                                  bufstr.substr(5));
                                    if (pendingRename[i].size() == 0) {
                                        sendall(i, MESSAGE_EXPECTING_RNFR, strlen(MESSAGE_EXPECTING_RNFR), 0);
                                    }
                                    if (rename(pendingRename[i].c_str(), (pathPair.first + pathPair.second).c_str())) {
                                        sendall(i, MESSAGE_RNTO_FAIL, strlen(MESSAGE_RNTO_FAIL), 0);
                                    } else {
                                        pendingRename[i].clear();
                                        sendall(i, MESSAGE_RNTO_SUCCESS, strlen(MESSAGE_RNTO_SUCCESS), 0);
                                    }
                                } else {
                                    sendall(i, MESSAGE_RNTO_FAIL, strlen(MESSAGE_RNTO_FAIL), 0);
                                }
                            }
                        }
                        else {
                            sendall(i, MESSAGE_UNKNOWN_COMMAND, strlen(MESSAGE_UNKNOWN_COMMAND), 0);
                        }
                    }
                }
            }
        }
    }

    return 0;
}