#ifndef QTCPK_HPP
#define QTCPK_HPP
#include <string>

void reportConnFailed(const std::string &alias, const std::string &msg);
void reportConnSuccess(const std::string &alias, int handle);
void reportListenSuccess(const std::string &alias, int protocol, int handle);
void reportDisconnect(int handle);
void reportMessage(int handle, const uint8_t *data, int len);
void reportNewClient(int handle, int newClient, const std::string &hostname);
void reportUdpListenSuccess(const std::string &alias, int handle);
void reportUdpMessage(int handle, int ip, int port, const uint8_t *data, int len);

#endif
