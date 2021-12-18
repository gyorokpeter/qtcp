#include "qtcp.hpp"
#include <unordered_map>
#include <sstream>
#include <vector>
#include <queue>
#include <memory>
#include <stdexcept>
#include <mutex>
#include <thread>
#include <w32api.h>
#undef _WIN32_WINNT
#define _WIN32_WINNT Windows7
#define WINVER Windows7
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#include <namedpipeapi.h>
#include <iostream>
#include "qtcpk.hpp"

int dumb_socketpair(SOCKET socks[2], int make_overlapped);

namespace {

    template<class ... Ts>
    std::string cat(Ts ... args) {
        std::ostringstream os;
        auto a = { (os << args, 0)... };
        if (sizeof(a) != sizeof(a)){}   //swallow the "unused variable" warning
        return os.str();
    }

    std::string niceWSAGetLastError() {
        int errcode = WSAGetLastError();
        char buffer[256];
        memset(buffer, 0, 256);
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, 255, 0);
        return cat("(", errcode,") ",buffer);
    }

    std::string niceGetLastError() {
        int errcode = GetLastError();
        char buffer[256];
        memset(buffer, 0, 256);
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, 255, 0);
        return cat("(", errcode,") ",buffer);
    }

    void drainPipe(int handle) {
        const int bufferSize = 65536;
        char buffer[bufferSize];
        int numBytesRead;
        //do {
            numBytesRead = recv(handle, buffer, bufferSize, 0);
        //} while (0<numBytesRead);
    }
}

class NetworkProcessor;
std::unordered_map<SOCKET, std::shared_ptr<NetworkProcessor> > readPipeToClient;
std::unordered_map<int, std::weak_ptr<NetworkProcessor> > handleToClient;

class NetworkProcessor {
public:
    enum Protocol { TCP, UDP };
    struct Msg {
        enum MsgType {
            ConnFailed,
            ConnSuccess,
            ListenSuccess,
            SendData,
            Disconnect,
            NewClient,
            UdpListenSuccess,
            SendUdpData
        } msgType;
        std::vector<uint8_t> data;
        Msg(MsgType msgType_, std::vector<uint8_t> data_) : msgType(msgType_), data(data_) {}
    };
private:
    bool isServer;
    std::string alias;
    std::string hoststr;
    std::string portstr;
    std::thread thread;
    void run();
    std::mutex upMutex;
    std::mutex downMutex;
    std::queue<Msg> upQueue;
    std::queue<Msg> downQueue;
    SOCKET hReadPipe, hWritePipe;
    Protocol protocol;
    int protocolVersion;
    int handle;

    void init() {
        SOCKET socks[2];
        if (dumb_socketpair(socks, 1) != 0) {
            throw std::runtime_error(niceWSAGetLastError());
        }
        hReadPipe = socks[0];
        hWritePipe = socks[1];
        setPipeNotify(int(hReadPipe));
        readPipeToClient[hReadPipe] = std::shared_ptr<NetworkProcessor>(this);
        thread = std::thread(&NetworkProcessor::run, this);
    }

public:
    NetworkProcessor(const std::string &alias_, const std::string &hoststr_, const std::string &portstr_) : isServer(false), alias(alias_), hoststr(hoststr_), portstr(portstr_), handle(0) {
        init();
    }
    NetworkProcessor(const std::string &alias_, Protocol protocol_, int protocolVersion_, int handle_, bool isServer_) : isServer(isServer_), alias(alias_), protocol(protocol_),
        protocolVersion(protocolVersion_), handle(handle_) {
        init();
    }
    ~NetworkProcessor() {
        thread.join();
        unsetPipeNotify(int(hReadPipe));
        handleToClient.erase(handle);
        readPipeToClient.erase(hReadPipe);
        if (closesocket(hReadPipe) != 0) std::cerr << "failed to close read pipe: " << niceWSAGetLastError() << std::endl;
        if (closesocket(hWritePipe) != 0) std::cerr << "failed to close write pipe: " << niceWSAGetLastError() << std::endl;
    }

    SOCKET getReadPipe() const { return hReadPipe; }

    void processMessages() {    //runs in main thread
        std::lock_guard<std::mutex> lock(upMutex);
        while (0<upQueue.size()) {
            const Msg &msg = upQueue.front();
            switch(msg.msgType) {
                case Msg::ConnFailed:
                    reportConnFailed(alias, std::string((char*)&msg.data[0], msg.data.size()));
                    std::cerr << "removing client on conn failed" << std::endl;
                    readPipeToClient.erase(hReadPipe);
                    return;
                case Msg::ConnSuccess:
                    handleToClient[handle] = readPipeToClient[hReadPipe];
                    reportConnSuccess(alias, handle);
                    break;
                case Msg::ListenSuccess:
                    handleToClient[handle] = readPipeToClient[hReadPipe];
                    reportListenSuccess(alias, protocolVersion, handle);
                    break;
                case Msg::Disconnect:
                    reportDisconnect(handle);
                    readPipeToClient.erase(hReadPipe);
                    return;
                case Msg::SendData:
                    reportMessage(handle, &msg.data[0], msg.data.size());
                    break;
                case Msg::NewClient:{
                    int newClient = *(int*)&msg.data[0];
                    std::string hostname = std::string(msg.data.begin()+sizeof(int), msg.data.end());
                    handleToClient[newClient] = readPipeToClient[(new NetworkProcessor(hostname+"."+std::to_string(newClient), NetworkProcessor::TCP, 0, newClient, false))->hReadPipe];
                    reportNewClient(handle, newClient, hostname);
                    break;
                }
                case Msg::UdpListenSuccess:
                    handleToClient[handle] = readPipeToClient[hReadPipe];
                    reportUdpListenSuccess(alias, handle);
                    break;
                case Msg::SendUdpData:{
                    int ip = *(int*)&msg.data[0];
                    int port = *(int*)&msg.data[sizeof(int)];
                    reportUdpMessage(handle, ip, port, &msg.data[2*sizeof(int)], msg.data.size()-2*sizeof(int));
                    break;
                }
                default:
                    std::cerr << "processMessages unknown message type " << int(msg.msgType) << std::endl;
            }
            upQueue.pop();
        }
    }

    void notifyPipeUp() {
        char buffer[1] = {0};
        int result = send(hWritePipe, buffer, 1, 0);
    }

    void notifyPipeDown() {
        char buffer[1] = {0};
        int result = send(hReadPipe, buffer, 1, 0);
    }

    void reportConnErrorUp(const std::string &msg) {
        std::cerr << "reporting error: " << msg << std::endl;
        std::lock_guard<std::mutex> lock(upMutex);
        upQueue.emplace(Msg::ConnFailed, std::vector<uint8_t>(msg.begin(), msg.end()));
        notifyPipeUp();
    }

    void reportDisconnectUp() {
        std::lock_guard<std::mutex> lock(upMutex);
        upQueue.emplace(Msg::Disconnect, std::vector<uint8_t>());
        notifyPipeUp();
    }

    void reportDisconnectDown() {
        std::lock_guard<std::mutex> lock(downMutex);
        downQueue.emplace(Msg::Disconnect, std::vector<uint8_t>());
        notifyPipeDown();
    }

    void reportConnSuccessUp() {
        std::lock_guard<std::mutex> lock(upMutex);
        upQueue.emplace(Msg::ConnSuccess, std::vector<uint8_t>());
        notifyPipeUp();
    }

    void reportListenSuccessUp() {
        std::lock_guard<std::mutex> lock(upMutex);
        upQueue.emplace(Msg::ListenSuccess, std::vector<uint8_t>());
        notifyPipeUp();
    }

    void reportUdpListenSuccessUp() {
        std::lock_guard<std::mutex> lock(upMutex);
        upQueue.emplace(Msg::UdpListenSuccess, std::vector<uint8_t>());
        notifyPipeUp();
    }

    void sendDataUp(const uint8_t *data, int size) {
        std::lock_guard<std::mutex> lock(upMutex);
        upQueue.emplace(Msg::SendData, std::vector<uint8_t>(data, data+size));
        notifyPipeUp();
    }

    void reportNewClientUp(int connSock, const std::string &hostname) {
        std::lock_guard<std::mutex> lock(upMutex);
        Msg &msg = upQueue.emplace(Msg::NewClient, std::vector<uint8_t>(sizeof(int)+hostname.length()));
        *(int*)&msg.data[0] = connSock;
        memcpy(&msg.data[sizeof(int)], &hostname[0], hostname.length());
        notifyPipeUp();
    }

    void sendDataDown(const uint8_t *data, int size) {
        if (isServer) throw std::runtime_error("can't send data on a listening handle");
        std::lock_guard<std::mutex> lock(downMutex);
        downQueue.emplace(Msg::SendData, std::vector<uint8_t>(data, data+size));
        notifyPipeDown();
    }

    void sendUdpDataDown(const std::string &hoststr, int port, const uint8_t *data, int size) {
        std::lock_guard<std::mutex> lock(downMutex);
        int hostLen = hoststr.size();
        std::vector<uint8_t> downmsg(2*sizeof(int)+size+hostLen);
        *(int*)&downmsg[0] = port;
        *(int*)&downmsg[sizeof(int)] = hostLen;
        memcpy(&downmsg[2*sizeof(int)], &hoststr[0], hostLen);
        memcpy(&downmsg[2*sizeof(int)+hostLen], data, size);
        downQueue.emplace(Msg::SendUdpData, std::move(downmsg));
        notifyPipeDown();
    }

    void sendUdpDataUp(int ip, int port, const uint8_t *data, int size) {
        std::lock_guard<std::mutex> lock(upMutex);
        int hostLen = hoststr.size();
        std::vector<uint8_t> upmsg(2*sizeof(int)+size);
        *(int*)&upmsg[0] = ip;
        *(int*)&upmsg[sizeof(int)] = port;
        memcpy(&upmsg[2*sizeof(int)], data, size);
        upQueue.emplace(Msg::SendUdpData, std::move(upmsg));
        notifyPipeUp();
    }

};

void NetworkProcessor::run() {
    if (handle == 0) {
        handle = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
        int ipv6only = 0;
        int iResult = setsockopt(handle, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&ipv6only, sizeof(ipv6only));
        if (iResult == SOCKET_ERROR){
            reportConnErrorUp("setsockopt: "+niceWSAGetLastError());
            closesocket(handle);
            return;
        }
        timeval timeout = { 60, 0 };
        bool connectResult = WSAConnectByName(handle, &hoststr[0], &portstr[0], 0, nullptr, 0, nullptr, &timeout, nullptr);
        if (!connectResult) {
            reportConnErrorUp("WSAConnectByName: "+niceWSAGetLastError());
            closesocket(handle);
            return;
        }
        reportConnSuccessUp();
    } else {    //handle != 0
        if (isServer) {
            if (protocol == TCP)
                reportListenSuccessUp();
            else
                reportUdpListenSuccessUp();
        }
    }

    WSAPOLLFD handlesForPoll[] = {
        {unsigned(hWritePipe),POLLRDNORM,0},
        {unsigned(handle),POLLRDNORM,0}
    };
    while (true) {
        int ret;
        if (SOCKET_ERROR == (ret = WSAPoll(handlesForPoll, 2, 30000)))
            { std::cerr << "WSAPoll: " << niceWSAGetLastError() << std::endl; }
        else {
            if ((handlesForPoll[0].revents & POLLHUP) || (handlesForPoll[0].revents & POLLNVAL)) {
                closesocket(handle);
                return;
            } else if (handlesForPoll[0].revents & POLLRDNORM) {
                drainPipe(hWritePipe);
                std::lock_guard<std::mutex> lock(downMutex);
                while (0<downQueue.size()) {
                    const Msg &msg = downQueue.front();
                    switch(msg.msgType) {
                        case Msg::ConnFailed:
                            break;
                        case Msg::ConnSuccess:
                            break;
                        case Msg::SendData:{
                            int offset = 0;
                            int toSend = msg.data.size();
                            while (toSend > 0) {
                                int result = send(handle, (const char*)&msg.data[offset], toSend, 0);
                                if (result <= 0) {
                                    std::cerr << "client send: " << niceWSAGetLastError() << " with " << toSend << "bytes left to send" << std::endl;
                                    break;
                                }
                                toSend -= result;
                                offset += result;
                            }
                            break;
                        }
                        case Msg::SendUdpData:{
                            int port = *(int*)&msg.data[0];
                            int hostLen = *(int*)&msg.data[sizeof(int)];
                            std::string hoststr = std::string(msg.data.begin()+2*sizeof(int), msg.data.begin()+2*sizeof(int)+hostLen);
                            hostent* hostResolved = gethostbyname(hoststr.c_str());
                            if (hostResolved == 0) {
                                std::cerr << "host resolution failed: " << niceWSAGetLastError() << std::endl;
                                break;
                            }
                            char *ip = inet_ntoa (*(struct in_addr *)*hostResolved->h_addr_list);
                            sockaddr_in addr;
                            addr.sin_family = AF_INET;
                            addr.sin_addr.s_addr = inet_addr(ip);
                            addr.sin_port = htons(port);

                            int offset = 2*sizeof(int)+hostLen;
                            int toSend = msg.data.size()-offset;
                            while (toSend > 0) {
                                int result = sendto(handle, (const char*)&msg.data[offset], toSend, 0, (sockaddr *)&addr, sizeof(addr));
                                if (result <= 0) {
                                    std::cerr << "client send: " << niceWSAGetLastError() << " with " << toSend << "bytes left to send" << std::endl;
                                    break;
                                }
                                toSend -= result;
                                offset += result;
                            }
                            break;
                        }
                        case Msg::Disconnect:
                            closesocket(handle);
                            return;
                        default:
                            std::cerr << "client: unknown message type " << int(msg.msgType) << std::endl;
                    }
                    downQueue.pop();
                }
            } else if ((handlesForPoll[1].revents & POLLHUP) || (handlesForPoll[1].revents & POLLNVAL)) {
                reportDisconnectUp();
                return;
            } else if (handlesForPoll[1].revents & POLLRDNORM) {
                if (protocol != UDP && !isServer) {
                    const int bufferSize = 65536;
                    char buffer[bufferSize];
                    int res = recv(handle, buffer, bufferSize, 0);
                    if (res < 0) {
                        std::cerr << "client receive " << niceWSAGetLastError() << std::endl;
                        closesocket(handle);
                        reportDisconnectUp();
                    } else {
                        sendDataUp((const uint8_t*)buffer, res);
                    }
                } else if (protocol != UDP && isServer) {
                    SOCKET ConnSock;
                    SOCKADDR_STORAGE From;
                    int FromLen = sizeof(From);
                    ConnSock = accept(handle, (LPSOCKADDR) &From, &FromLen);
                    if (ConnSock == INVALID_SOCKET) {
                        std::cerr << "accept: " << niceWSAGetLastError() << std::endl;
                    } else {
                        char Hostname[NI_MAXHOST] = "<unknown>";
                        getnameinfo((LPSOCKADDR) &From, FromLen, Hostname,
                                sizeof(Hostname), NULL, 0, NI_NUMERICHOST);
                        reportNewClientUp(ConnSock, Hostname);
                    }
                } else {    //protocol==UDP
                    sockaddr_in addr;
                    int addrsize = sizeof(addr);
                    const int bufferSize = 65536;
                    char buffer[bufferSize];
                    int res = recvfrom(handle, buffer, bufferSize, 0, (sockaddr *) &addr, &addrsize);
                    if (SOCKET_ERROR == res) {
                        std::cerr << "recvfrom " << niceWSAGetLastError() << std::endl;
                    } else {
                        int ip = ntohl(addr.sin_addr.s_addr);
                        int port = ntohs(addr.sin_port);
                        sendUdpDataUp(ip, port, (const uint8_t*)buffer, res);
                    }
                }
            }
        }
    }
}

void addConnection(const std::string &alias, const std::string &hoststr, const std::string &portstr) {
    new NetworkProcessor(alias, hoststr, portstr);
}

std::string addListener(const std::string &alias, const std::string &portstr) {
    std::vector<std::string> errors;
    int iResult;
    ADDRINFO Hints, *AddrInfo;
    memset(&Hints, 0, sizeof (Hints));
    Hints.ai_family = PF_UNSPEC;
    Hints.ai_socktype = SOCK_STREAM;
    Hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;
    char *Address = nullptr;
    iResult = getaddrinfo(Address, &portstr[0], &Hints, &AddrInfo);
    if (iResult != 0) {
        throw std::runtime_error("getaddrinfo: " + niceWSAGetLastError());
    }
    int succ = 0;
    for (ADDRINFO *AI = AddrInfo; AI != NULL; AI = AI->ai_next) {
        if ((AI->ai_family != PF_INET) && (AI->ai_family != PF_INET6))
            continue;
        int handle = socket(AI->ai_family, AI->ai_socktype, AI->ai_protocol);
        if (handle == INVALID_SOCKET) {
            errors.emplace_back("socket: " + niceWSAGetLastError());
            continue;
        }
        int iOptval = 1;
        int iResult = setsockopt(handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                         (char *) &iOptval, sizeof (iOptval));
        if (iResult == SOCKET_ERROR) {
            errors.emplace_back("setsockopt: " + niceWSAGetLastError());
        }
        if (bind(handle, AI->ai_addr, int(AI->ai_addrlen)) == SOCKET_ERROR) {
            errors.emplace_back("bind: " + niceWSAGetLastError());
            closesocket(handle);
            continue;
        }
        if (listen(handle, SOMAXCONN) == SOCKET_ERROR) {
            errors.emplace_back("listen: " + niceWSAGetLastError());
            closesocket(handle);
            continue;
        }
        ++succ;
        new NetworkProcessor(alias, NetworkProcessor::TCP, AI->ai_family == PF_INET ? 4 : 6, handle, true);
    }
    freeaddrinfo(AddrInfo);
    std::ostringstream errs;
    for (auto s : errors) errs << s << std::endl;
    if(0==succ) {
        throw std::runtime_error("no sockets successfully listening:\n" + errs.str());
    }
    return errs.str();
}

void addUdpListener(const std::string &alias, int port) {
    int handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int iOptval = 1;
    int iResult = setsockopt(handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                     (char *) &iOptval, sizeof (iOptval));
    if (iResult == SOCKET_ERROR) {
        std::cerr << "in setsockopt: " << niceWSAGetLastError() << std::endl;
    }
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    int bindResult = bind(handle, (sockaddr *)&addr, sizeof(addr));
    if (0 != bindResult) {
        std::string msg = niceWSAGetLastError();
        closesocket(handle);
        throw std::runtime_error(msg);
    } else {
        new NetworkProcessor(alias, NetworkProcessor::UDP, 0, handle, true);
        //k(-kdbhandle, (char*)".udp.listenSuccess", kp((char*)aliasstr.c_str()), ki(handle), K(0));
        //if (debug) cout << ".udp.listen: handle=" << handle << endl;
        //queueHandleToPlace(handle, PROXY_UDP);
    }
}

void closeConnection(int handle) {
    auto clientIt = handleToClient.find(handle);
    if (clientIt == handleToClient.end()) throw std::runtime_error("invalid handle");
    auto client = clientIt->second.lock();
    if (!client) {
        handleToClient.erase(handle);
        throw std::runtime_error("client was destroyed but not removed from handleToClient?!");
    }
    client->reportDisconnectDown();
    readPipeToClient.erase(client->getReadPipe());
    reportDisconnect(handle);
}

void sendData(int handle, const uint8_t *data, int len) {
    auto clientIt = handleToClient.find(handle);
    if (clientIt == handleToClient.end()) throw std::runtime_error("invalid handle");
    auto client = clientIt->second.lock();
    if (!client) {
        handleToClient.erase(handle);
        throw std::runtime_error("client was destroyed but not removed from handleToClient?!");
    }
    client->sendDataDown(data, len);
}

void sendUdpData(int handle, const std::string &hoststr, int port, const uint8_t *data, int len) {
    auto clientIt = handleToClient.find(handle);
    if (clientIt == handleToClient.end()) throw std::runtime_error("invalid handle");
    auto client = clientIt->second.lock();
    if (!client) {
        handleToClient.erase(handle);
        throw std::runtime_error("client was destroyed but not removed from handleToClient?!");
    }
    client->sendUdpDataDown(hoststr, port, data, len);
}

std::shared_ptr<NetworkProcessor> getClientForPipe(SOCKET handle) {
    auto item = readPipeToClient.find(handle);
    if (item == readPipeToClient.end()) return {};
    return item->second;
}

void pipeNotify(int handle) {
    drainPipe(handle);
    std::shared_ptr<NetworkProcessor> client = getClientForPipe(SOCKET(handle));
    if (!client) {
        std::cerr << "pipeNotify called for invalid handle " << handle << std::endl;
        return;
    }
    client->processMessages();
}
