// Target Win10/11 so IP_ADAPTER_ADDRESSES_LH exposes Dhcpv4Server
#ifndef _WIN32_WINNT
#  define _WIN32_WINNT  0x0A00
#endif
#ifndef NTDDI_VERSION
#  define NTDDI_VERSION 0x0A000000
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// winsock2 must precede windows.h
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <wincrypt.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ws2_32.lib")

#include "NetworkInfo.h"
#include <memory>
#include <cstdio>
#include <cstring>

static void W2U(WCHAR* wsz, char* dst, int dstLen)
{
    if (!wsz) { dst[0] = '\0'; return; }
    WideCharToMultiByte(CP_UTF8, 0, wsz, -1, dst, dstLen, nullptr, nullptr);
}

static void Ip4Str(const void* pAddr, char* dst, int dstLen)
{
    // Wrapper so we never collide with the math-function name 'sin'
    inet_ntop(AF_INET, pAddr, dst, (socklen_t)dstLen);
}

// ── Adapter enumeration ────────────────────────────────────────────────────────

std::vector<AdapterInfo> GetAllAdapters()
{
    const ULONG kFlags = GAA_FLAG_INCLUDE_GATEWAYS |
                         GAA_FLAG_SKIP_ANYCAST      |
                         GAA_FLAG_SKIP_MULTICAST;
    ULONG bufLen = 16000;
    std::unique_ptr<BYTE[]> buf;
    ULONG ret;
    do {
        buf = std::make_unique<BYTE[]>(bufLen);
        ret = GetAdaptersAddresses(AF_INET, kFlags, nullptr,
                                   (PIP_ADAPTER_ADDRESSES)buf.get(), &bufLen);
    } while (ret == ERROR_BUFFER_OVERFLOW);

    if (ret != NO_ERROR) return {};

    std::vector<AdapterInfo> result;
    for (PIP_ADAPTER_ADDRESSES pa = (PIP_ADAPTER_ADDRESSES)buf.get(); pa; pa = pa->Next) {
        if (pa->OperStatus != IfOperStatusUp)           continue;
        if (pa->IfType    == IF_TYPE_SOFTWARE_LOOPBACK) continue;

        AdapterInfo ai;
        char tmp[260] = {};

        W2U(pa->FriendlyName, tmp, sizeof(tmp));
        ai.friendlyName = tmp;

        if (pa->DnsSuffix && pa->DnsSuffix[0]) {
            W2U(pa->DnsSuffix, tmp, sizeof(tmp));
            ai.dnsSuffix = tmp;
        }

        ai.dhcpEnabled = (pa->Flags & IP_ADAPTER_DHCP_ENABLED) != 0;

        // First IPv4 unicast address and subnet mask from prefix length
        for (PIP_ADAPTER_UNICAST_ADDRESS pu = pa->FirstUnicastAddress; pu; pu = pu->Next) {
            if (pu->Address.lpSockaddr->sa_family != AF_INET) continue;
            sockaddr_in* pA4 = (sockaddr_in*)pu->Address.lpSockaddr;
            Ip4Str(&pA4->sin_addr, tmp, sizeof(tmp));
            ai.ip = tmp;

            UINT8 plen  = pu->OnLinkPrefixLength;
            ULONG bits  = plen ? htonl(0xFFFFFFFFul << (32 - plen)) : 0ul;
            in_addr msk; msk.s_addr = bits;
            Ip4Str(&msk, tmp, sizeof(tmp));
            ai.subnet = tmp;
            break;
        }

        // Default gateway
        if (pa->FirstGatewayAddress) {
            SOCKADDR* pGsa = pa->FirstGatewayAddress->Address.lpSockaddr;
            if (pGsa->sa_family == AF_INET) {
                Ip4Str(&((sockaddr_in*)pGsa)->sin_addr, tmp, sizeof(tmp));
                ai.gateway = tmp;
            }
        }

        // DHCP server (from active lease; available in Vista+ struct)
#if (NTDDI_VERSION >= NTDDI_VISTA)
        if (ai.dhcpEnabled && pa->Dhcpv4Server.lpSockaddr) {
            SOCKADDR* pDsa = pa->Dhcpv4Server.lpSockaddr;
            if (pDsa->sa_family == AF_INET) {
                Ip4Str(&((sockaddr_in*)pDsa)->sin_addr, tmp, sizeof(tmp));
                ai.dhcpServer = tmp;
            }
        }
#endif

        ai.isConnected = !ai.ip.empty() && ai.ip != "0.0.0.0";
        if (ai.isConnected) result.push_back(std::move(ai));
    }
    return result;
}

AdapterInfo GetPrimaryAdapter()
{
    std::vector<AdapterInfo> all = GetAllAdapters();
    for (size_t i = 0; i < all.size(); ++i) if (all[i].dhcpEnabled && all[i].isConnected) return all[i];
    for (size_t i = 0; i < all.size(); ++i) if (all[i].isConnected)                       return all[i];
    return AdapterInfo{};
}

// ── Machine certificate discovery (AD CS auto-enrollment) ─────────────────────

MachineCredential FindMachineCert()
{
    MachineCredential mc;
    HCERTSTORE hStore = CertOpenSystemStoreW(0, L"MY");
    if (!hStore) return mc;

    FILETIME bestExpiry = {};
    PCCERT_CONTEXT pEnum = nullptr;

    while ((pEnum = CertEnumCertificatesInStore(hStore, pEnum)) != nullptr) {
        // Require Server Authentication EKU (1.3.6.1.5.5.7.3.1)
        DWORD ekuSz = 0;
        CertGetEnhancedKeyUsage(pEnum, 0, nullptr, &ekuSz);
        if (!ekuSz) continue;

        std::unique_ptr<BYTE[]> ekuBuf(new BYTE[ekuSz]);
        CERT_ENHKEY_USAGE* pEku = (CERT_ENHKEY_USAGE*)ekuBuf.get();
        if (!CertGetEnhancedKeyUsage(pEnum, 0, pEku, &ekuSz)) continue;

        bool hasServerAuth = false;
        for (DWORD i = 0; i < pEku->cUsageIdentifier; ++i)
            if (strcmp(pEku->rgpszUsageIdentifier[i], szOID_PKIX_KP_SERVER_AUTH) == 0)
                { hasServerAuth = true; break; }
        if (!hasServerAuth) continue;

        FILETIME now;
        GetSystemTimeAsFileTime(&now);
        if (CompareFileTime(&pEnum->pCertInfo->NotAfter,  &now) < 0) continue;
        if (CompareFileTime(&pEnum->pCertInfo->NotBefore, &now) > 0) continue;

        if (mc.found && CompareFileTime(&pEnum->pCertInfo->NotAfter, &bestExpiry) <= 0)
            continue;

        if (mc.certContext)
            CertFreeCertificateContext((PCCERT_CONTEXT)mc.certContext);

        bestExpiry     = pEnum->pCertInfo->NotAfter;
        mc.found       = true;
        mc.certContext = (void*)CertDuplicateCertificateContext(pEnum);

        char buf[512] = {};
        CertGetNameStringA(pEnum, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                           0, nullptr, buf, sizeof(buf));
        mc.subjectCN = buf;

        CertGetNameStringA(pEnum, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                           CERT_NAME_ISSUER_FLAG, nullptr, buf, sizeof(buf));
        mc.issuerCN = buf;

        BYTE thumb[20] = {};
        DWORD tsz = sizeof(thumb);
        CertGetCertificateContextProperty(pEnum, CERT_SHA1_HASH_PROP_ID, thumb, &tsz);
        char hex[41] = {};
        for (int i = 0; i < 20; ++i)
            std::snprintf(hex + i * 2, 3, "%02x", thumb[i]);
        mc.thumbprint = hex;

        SYSTEMTIME st = {};
        FileTimeToSystemTime(&bestExpiry, &st);
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
        mc.validUntil = buf;
    }

    CertCloseStore(hStore, 0);
    return mc;
}

void FreeMachineCert(MachineCredential& mc)
{
    if (mc.certContext) {
        CertFreeCertificateContext((PCCERT_CONTEXT)mc.certContext);
        mc.certContext = nullptr;
    }
    mc.found = false;
}

// ── AD CS enrollment (CertEnroll COM) ─────────────────────────────────────────
// certenroll.h needs COM/OLE2 types; include objbase.h before it.
#include <objbase.h>
#include <certenroll.h>

bool TryEnrollMachineCert(std::string& statusOut)
{
    // Initialise COM for this call (STA, matching the wx UI thread).
    // S_FALSE = already initialised by caller — do not uninit.
    HRESULT hrCo  = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool didInit  = (hrCo == S_OK);

    // Template names to try in preference order.
    // "WebServer" carries Server Authentication EKU and allows the app to use
    // the cert for HTTPS.  "Machine"/"Computer" are the default computer
    // templates and also carry Server Authentication on most AD CS configs.
    static const wchar_t* kTemplates[] = {
        L"WebServer", L"Machine", L"Computer", nullptr
    };

    bool enrolled = false;
    for (int ti = 0; kTemplates[ti] && !enrolled; ++ti) {
        IX509Enrollment* pEnroll = nullptr;
        HRESULT hr = CoCreateInstance(
            __uuidof(CX509Enrollment), nullptr, CLSCTX_INPROC_SERVER,
            __uuidof(IX509Enrollment), (void**)&pEnroll);
        if (FAILED(hr)) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "CX509Enrollment unavailable (0x%08X)", (unsigned)hr);
            statusOut = buf;
            break;     // COM server not registered — no point retrying other templates
        }

        BSTR bsTmpl = SysAllocString(kTemplates[ti]);
        hr = pEnroll->InitializeFromTemplateName(ContextMachine, bsTmpl);
        SysFreeString(bsTmpl);

        if (SUCCEEDED(hr)) {
            hr = pEnroll->Enroll();
            if (SUCCEEDED(hr)) {
                char narrow[64] = {};
                WideCharToMultiByte(CP_UTF8, 0, kTemplates[ti], -1,
                                    narrow, (int)sizeof(narrow), nullptr, nullptr);
                statusOut = std::string("AD CS cert enrolled (template: ") + narrow + ")";
                enrolled  = true;
            }
        }
        pEnroll->Release();
    }

    if (!enrolled && statusOut.empty())
        statusOut = "AD CS: no domain CA reachable or no applicable template";

    if (didInit) CoUninitialize();
    return enrolled;
}

// ── Machine identity ──────────────────────────────────────────────────────────

std::string GetMachineFqdn()
{
    WCHAR buf[256] = {};
    DWORD sz = 256;
    GetComputerNameExW(ComputerNameDnsFullyQualified, buf, &sz);
    char out[256] = {};
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, out, sizeof(out), nullptr, nullptr);
    return out;
}

std::string GetDomainName()
{
    WCHAR buf[256] = {};
    DWORD sz = 256;
    GetComputerNameExW(ComputerNameDnsDomain, buf, &sz);
    char out[256] = {};
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, out, sizeof(out), nullptr, nullptr);
    return out;
}
