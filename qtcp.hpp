#ifndef QTCP_HPP
#define QTCP_HPP

#include <string>

void addConnection(const std::string &alias, const std::string &hoststr, const std::string &portstr);
std::string addListener(const std::string &alias, const std::string &portstr);
void addUdpListener(const std::string &alias, int port);
void closeConnection(int handle);
void sendData(int handle, const uint8_t *data, int len);
void sendUdpData(int handle, const std::string &hoststr, int port, const uint8_t *data, int len);

void pipeNotify(int handle);

#endif
