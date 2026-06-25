#pragma once
#include <string>
#include <vector>

struct AdapterInfo {
    std::string friendlyName;
    std::string ip;           // DHCP-assigned IPv4
    std::string subnet;       // prefix length as mask string
    std::string gateway;
    std::string dhcpServer;
    std::string dnsSuffix;    // e.g. "domain.mil"
    bool        dhcpEnabled  = false;
    bool        isConnected  = false;
};

// Machine certificate from the LOCAL machine MY store (AD CS auto-enrolled).
// certContext is PCCERT_CONTEXT cast to void*; caller must CertFreeCertificateContext it.
struct MachineCredential {
    bool        found      = false;
    std::string subjectCN;
    std::string issuerCN;
    std::string thumbprint;   // SHA-1 hex, 40 chars
    std::string validUntil;   // ISO date
    void*       certContext = nullptr;
};

std::vector<AdapterInfo> GetAllAdapters();
AdapterInfo              GetPrimaryAdapter();    // first non-loopback, connected, DHCP adapter
MachineCredential        FindMachineCert();      // Server Authentication EKU, longest validity
void                     FreeMachineCert(MachineCredential& mc); // safe even when !mc.found
// Request a Server Authentication cert from the domain AD CS CA via CertEnroll COM.
// Returns true and sets statusOut on success; cert is written to LocalMachine\My.
// Non-fatal: returns false if domain CA is unreachable or machine is not domain-joined.
bool                     TryEnrollMachineCert(std::string& statusOut);
std::string              GetMachineFqdn();       // e.g. "ops01.domain.mil"
std::string              GetDomainName();        // e.g. "domain.mil"
