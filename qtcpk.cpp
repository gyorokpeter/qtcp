#include <string>
#include <map>
#include <stdexcept>
#include <iostream>
#include "qtcp.hpp"
#define KXVER 3
#include <k.h>

namespace {

    inline K kerror(const char *err) {
        return krr(const_cast<S>(err));
    }

    inline int k2int(K kobj) {
        return kobj->t == -6?kobj->i:kobj->j;
    }

    std::string k2str(K kobj) {
        return kobj->t == -11?std::string(kobj->s):std::string((char*)kC(kobj),kobj->n);
    }

}

void reportConnFailed(const std::string &alias, const std::string &msg) {
    K r = k(0, (char*)".tcp.connFailed", kp((char*)alias.c_str()), kp((char*)msg.c_str()), K(0));
    if (r != nullptr) {
        if (r->t == -128) {
            std::cerr << ".tcp.connFailed threw error: " << r->s << std::endl;
        }
        r0(r);
    }
}

void reportConnSuccess(const std::string &alias, int handle) {
    K r = k(0, (char*)".tcp.connSuccess", kp((char*)alias.c_str()), ki(handle), K(0));
    if (r != nullptr) {
        if (r->t == -128) {
            std::cerr << ".tcp.connSuccess threw error: " << r->s << std::endl;
        }
        r0(r);
    }
}

void reportListenSuccess(const std::string &alias, int protocol, int handle) {
    K r = k(0, (char*)".tcp.listenSuccess", kp((char*)alias.c_str()), ki(protocol), ki(handle), K(0));
    if (r != nullptr) {
        if (r->t == -128) {
            std::cerr << ".tcp.listenSuccess threw error: " << r->s << std::endl;
        }
        r0(r);
    }
}

void reportDisconnect(int handle) {
    K r = k(0, (char*)".tcp.disconnect", ki(handle), K(0));
    if (r != nullptr) {
        if (r->t == -128) {
            std::cerr << ".tcp.disconnect threw error: " << r->s << std::endl;
        }
        r0(r);
    }
}

void reportMessage(int handle, const uint8_t *data, int len) {
    K datak = ktn(KG, len);
    memcpy(kG(datak), data, len);
    K r = k(0, (char*)".tcp.receive", ki(handle), datak, K(0));
    if (r != nullptr) {
        if (r->t == -128) {
            std::cerr << ".tcp.receive threw error: " << r->s << std::endl;
        }
        r0(r);
    }
}

void reportNewClient(int handle, int newClient, const std::string &hostname) {
    K r = k(0, (char*)".tcp.clientConnect", ki(handle), ki(newClient), kp((char*)hostname.c_str()), K(0));
    if (r != nullptr) {
        if (r->t == -128) {
            std::cerr << ".tcp.clientConnect threw error: " << r->s << std::endl;
        }
        r0(r);
    }
}

void reportUdpListenSuccess(const std::string &alias, int handle) {
    K r = k(0, (char*)".udp.listenSuccess", kp((char*)alias.c_str()), ki(handle), K(0));
    if (r != nullptr) {
        if (r->t == -128) {
            std::cerr << ".udp.listenSuccess threw error: " << r->s << std::endl;
        }
        r0(r);
    }
}

void reportUdpMessage(int handle, int ip, int port, const uint8_t *data, int len) {
    K datak = ktn(KG, len);
    memcpy(kG(datak), data, len);
    K r = k(0, (char*)".udp.receive", ki(handle), ki(ip), ki(port), datak, K(0));
    if (r != nullptr) {
        if (r->t == -128) {
            std::cerr << ".udp.receive threw error: " << r->s << std::endl;
        }
        r0(r);
    }
}


extern "C" {

K qtcp_connect(K alias, K host, K port) {
    if (! (alias->t == -11 || alias->t == 10)) return kerror("wrong type for alias");
    if (! (host->t == -11 || host->t == 10)) return kerror("wrong type for host");
    if (! (port->t == -6 || port->t == -7)) return kerror("wrong type for port");
    std::string aliasstr = k2str(alias);
    std::string hoststr = k2str(host);
    std::string portstr = std::to_string(k2int(port));
    try {
        addConnection(aliasstr, hoststr, portstr);
    } catch(const std::exception &e) {
        return kerror(e.what());
    }
    return K(0);
}

K qtcp_close(K handle) {
    if (! (handle->t == -6 || handle->t == -7)) return kerror("wrong type for handle");
    try {
        closeConnection(k2int(handle));
    } catch(const std::exception &e) {
        return kerror(e.what());
    }
    return K(0);
}

K qtcp_send(K handle, K data) {
    if (! (handle->t == -6 || handle->t == -7)) return kerror("wrong type for handle");
    if (data->t != KG) return kerror("wrong type for data");
    try {
        sendData(k2int(handle), kG(data), data->n);
    } catch(const std::exception &e) {
        return kerror(e.what());
    }
    return K(0);
}

K qtcp_listen(K alias, K port) {
    if (! (alias->t == -11 || alias->t == 10)) return kerror("wrong type for alias");
    if (! (port->t == -6 || port->t == -7)) return kerror("wrong type for port");
    std::string aliasstr = k2str(alias);
    std::string portstr = std::to_string(k2int(port));
    try {
        std::string errs = addListener(aliasstr, portstr);
        return kpn(&errs[0], errs.size());
    } catch(const std::exception &e) {
        return kerror(e.what());
    }
}

K qudp_listen(K alias, K port) {
    if (! (alias->t == -11 || alias->t == 10)) return kerror("wrong type for alias");
    if (! (port->t == -6 || port->t == -7)) return kerror("wrong type for port");
    std::string aliasstr = k2str(alias);
    try {
        addUdpListener(aliasstr, k2int(port));
    } catch(const std::exception &e) {
        return kerror(e.what());
    }
    return K(0);
}

K qudp_send(K handle, K host, K port, K data) {
    if (! (handle->t == -6 || handle->t == -7)) return kerror("wrong type for handle");
     if (! (host->t == -11 || host->t == 10)) return kerror("wrong type for host");
    if (! (port->t == -6 || port->t == -7)) return kerror("wrong type for port");
    std::string hoststr = k2str(host);
   if (data->t != KG) return kerror("wrong type for data");
    try {
        sendUdpData(k2int(handle), hoststr, k2int(port), kG(data), data->n);
    } catch(const std::exception &e) {
        return kerror(e.what());
    }
    return K(0);
}

}