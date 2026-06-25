#pragma once
#include "EventStore.h"
#include "SatelliteStore.h"
#include "GroundStationStore.h"
#include "UserStore.h"
#include "TleStore.h"
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <vector>
#include <map>
#include <functional>

// Embedded HTTP/1.1 REST server for multi-operator LAN sharing.
// Listens on port 7743 by default.  All store access is guarded by mutex_.
class LanServer {
public:
    LanServer(EventStore&        ev,
              SatelliteStore&    sat,
              GroundStationStore& gs,
              UserStore&         usr,
              TleStore&          tle,
              std::function<bool(const std::string&, const std::string&)> authFn);
    ~LanServer();

    bool Start(int port, std::string& err);
    void Stop();

    bool        IsRunning()   const { return running_.load(); }
    bool        IsTlsEnabled()const { return tlsEnabled_; }
    int         Port()        const { return port_; }
    std::string LocalIp()     const;

    // Called by CalendarFrame's UI thread to signal connected clients to refresh.
    // (Currently informational; clients poll on a timer.)
    void NotifyChanged() {}

private:
    void ListenThread();
    void HandleConnection(uintptr_t sock);   // uintptr_t avoids pulling in winsock2.h here

    std::string Dispatch(const std::string& method,
                         const std::string& path,
                         const std::string& body,
                         const std::string& token,
                         int& statusOut);

    std::string HandleAuth   (const std::string& body);
    std::string HandleEvents (const std::string& method, const std::string& id, const std::string& body, int& st);
    std::string HandleSats   (const std::string& method, const std::string& id, const std::string& body, int& st);
    std::string HandleGs     (const std::string& method, const std::string& id, const std::string& body, int& st);
    std::string HandleUsers  (int& st);

    bool ValidateToken(const std::string& token) const;
    std::string IssueToken(const std::string& user);

    EventStore&         ev_;
    SatelliteStore&     sat_;
    GroundStationStore& gs_;
    UserStore&          usr_;
    TleStore&           tle_;
    std::function<bool(const std::string&, const std::string&)> authFn_;

    mutable std::mutex            mutex_;
    mutable std::mutex            tokenMutex_;
    std::map<std::string,std::string> tokens_;   // token → username

    // Thread pool — one accept thread, N worker threads (2× hw_concurrency, min 4, max 32)
    void WorkerThread();
    void EnqueueSocket(uintptr_t sock);

    std::vector<std::thread>    workers_;
    std::queue<uintptr_t>       workQueue_;
    std::mutex                  workMutex_;
    std::condition_variable     workCv_;

    std::thread         acceptThread_;
    std::atomic<bool>   running_{false};
    uintptr_t           listenSock_     = (uintptr_t)~0ull;
    int                 port_           = 7743;
    int                 threadCount_    = 0;

    // TLS — heap-allocated CredHandle (avoid pulling sspi.h into header)
    bool  tlsEnabled_    = false;
    void* tlsCredHandle_ = nullptr;
};
