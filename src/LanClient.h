#pragma once
#include <string>

// Lightweight WinHTTP-based JSON REST client.
// Used by stores when running in LAN client mode.
class LanClient {
public:
    LanClient() = default;
    ~LanClient();

    // Connect (validates the server is reachable); sets host/port for all subsequent calls.
    bool Connect(const std::string& host, int port, std::string& err);

    // Authenticate with the server; stores the bearer token internally.
    bool Auth(const std::string& user, const std::string& pass, std::string& err);

    std::string Get   (const std::string& path, std::string& err);
    std::string Post  (const std::string& path, const std::string& body, std::string& err);
    std::string Put   (const std::string& path, const std::string& body, std::string& err);
    bool        Delete(const std::string& path, std::string& err);

    bool        IsConnected() const { return connected_; }
    std::string Host()        const { return host_; }
    int         Port()        const { return port_; }

private:
    std::string Request(const std::string& method, const std::string& path,
                        const std::string& body, std::string& err);

    std::string host_;
    int         port_      = 7743;
    std::string token_;
    bool        connected_ = false;
};
