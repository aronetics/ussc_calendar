#ifdef _WIN32
#include "OutlookComConnector.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <oleauto.h>
#include <string>

// ── COM dispatch helpers ──────────────────────────────────────────────────────

static HRESULT ComGetProperty(IDispatch* pObj, const wchar_t* name,
                               VARIANT* pResult)
{
    DISPID dispId = 0;
    LPOLESTR member = const_cast<LPOLESTR>(name);
    HRESULT hr = pObj->GetIDsOfNames(IID_NULL, &member, 1,
                                      LOCALE_USER_DEFAULT, &dispId);
    if (FAILED(hr)) return hr;

    DISPPARAMS dp = {};
    VariantInit(pResult);
    return pObj->Invoke(dispId, IID_NULL, LOCALE_USER_DEFAULT,
                        DISPATCH_PROPERTYGET, &dp, pResult, nullptr, nullptr);
}

static HRESULT ComCallMethod(IDispatch* pObj, const wchar_t* name,
                              VARIANT* args, int argCount, VARIANT* pResult)
{
    DISPID dispId = 0;
    LPOLESTR member = const_cast<LPOLESTR>(name);
    HRESULT hr = pObj->GetIDsOfNames(IID_NULL, &member, 1,
                                      LOCALE_USER_DEFAULT, &dispId);
    if (FAILED(hr)) return hr;

    DISPPARAMS dp = {};
    dp.cArgs = argCount;
    dp.rgvarg = args;

    if (pResult) VariantInit(pResult);
    return pObj->Invoke(dispId, IID_NULL, LOCALE_USER_DEFAULT,
                        DISPATCH_METHOD, &dp, pResult, nullptr, nullptr);
}

static IDispatch* ComGetDispatch(IDispatch* pObj, const wchar_t* prop)
{
    VARIANT v;
    if (FAILED(ComGetProperty(pObj, prop, &v))) return nullptr;
    if (v.vt != VT_DISPATCH || !v.pdispVal) { VariantClear(&v); return nullptr; }
    return v.pdispVal;  // caller AddRef's if needed
}

static std::wstring VariantToWString(const VARIANT& v)
{
    if (v.vt == VT_BSTR && v.bstrVal)
        return std::wstring(v.bstrVal);
    return {};
}

static std::string WideToUtf8(const std::wstring& ws)
{
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1,
                                   nullptr, 0, nullptr, nullptr);
    std::string s(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1,
                         &s[0], len, nullptr, nullptr);
    return s;
}

// Parse "MM/DD/YYYY HH:MM" or ISO variant from Outlook date strings
static wxDateTime ParseOutlookDate(const std::wstring& ws)
{
    std::string s = WideToUtf8(ws);
    wxDateTime dt;
    // Try ISO first
    if (dt.ParseFormat(s, "%Y-%m-%dT%H:%M:%S")) return dt;
    if (dt.ParseFormat(s, "%m/%d/%Y %H:%M"))    return dt;
    dt.ParseDateTime(s);
    return dt;
}

// ── Connector implementation ──────────────────────────────────────────────────

bool OutlookComConnector::Connect(std::string& errorMsg)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    CLSID clsid;
    HRESULT hr = CLSIDFromProgID(L"Outlook.Application", &clsid);
    if (FAILED(hr)) {
        errorMsg = "Outlook is not installed or not registered on this system.";
        return false;
    }

    IDispatch* pApp = nullptr;
    hr = CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER,
                           IID_IDispatch, reinterpret_cast<void**>(&pApp));
    if (FAILED(hr) || !pApp) {
        // Try GetActiveObject first (Outlook already running)
        IUnknown* pUnk = nullptr;
        if (SUCCEEDED(GetActiveObject(clsid, nullptr, &pUnk)) && pUnk) {
            pUnk->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(&pApp));
            pUnk->Release();
        }
    }
    if (!pApp) {
        errorMsg = "Could not start or attach to Outlook. Ensure Outlook is installed.";
        return false;
    }
    pApp->Release();
    connected_ = true;
    return true;
}

std::vector<CalendarEvent> OutlookComConnector::FetchEvents(
    const wxDateTime& from, const wxDateTime& to, std::string& errorMsg)
{
    std::vector<CalendarEvent> results;

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    CLSID clsid;
    CLSIDFromProgID(L"Outlook.Application", &clsid);

    IDispatch* pApp = nullptr;
    HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER,
                                   IID_IDispatch, reinterpret_cast<void**>(&pApp));
    if (FAILED(hr) || !pApp) {
        IUnknown* pUnk = nullptr;
        if (SUCCEEDED(GetActiveObject(clsid, nullptr, &pUnk)) && pUnk) {
            pUnk->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(&pApp));
            pUnk->Release();
        }
    }
    if (!pApp) { errorMsg = "Cannot connect to Outlook."; return results; }

    // GetNamespace("MAPI")
    VARIANT argMapi;
    VariantInit(&argMapi);
    argMapi.vt = VT_BSTR;
    argMapi.bstrVal = SysAllocString(L"MAPI");
    VARIANT vNS;
    ComCallMethod(pApp, L"GetNamespace", &argMapi, 1, &vNS);
    SysFreeString(argMapi.bstrVal);

    IDispatch* pNS = (vNS.vt == VT_DISPATCH) ? vNS.pdispVal : nullptr;
    if (!pNS) { errorMsg = "Cannot get MAPI namespace."; pApp->Release(); return results; }

    // GetDefaultFolder(olFolderCalendar = 9)
    VARIANT argFolder;
    VariantInit(&argFolder);
    argFolder.vt = VT_I4;
    argFolder.lVal = 9;  // olFolderCalendar
    VARIANT vFolder;
    ComCallMethod(pNS, L"GetDefaultFolder", &argFolder, 1, &vFolder);

    IDispatch* pCalFolder = (vFolder.vt == VT_DISPATCH) ? vFolder.pdispVal : nullptr;
    if (!pCalFolder) { errorMsg = "Cannot open Calendar folder."; pNS->Release(); pApp->Release(); return results; }

    // Get Items collection
    IDispatch* pItems = ComGetDispatch(pCalFolder, L"Items");
    if (!pItems) { errorMsg = "Cannot get calendar items."; pCalFolder->Release(); pNS->Release(); pApp->Release(); return results; }

    // Include recurrences, sort by start
    VARIANT vTrue; VariantInit(&vTrue); vTrue.vt = VT_BOOL; vTrue.boolVal = VARIANT_TRUE;
    ComCallMethod(pItems, L"IncludeRecurrences", &vTrue, 1, nullptr);

    VARIANT vSortField; VariantInit(&vSortField);
    vSortField.vt = VT_BSTR; vSortField.bstrVal = SysAllocString(L"[Start]");
    ComCallMethod(pItems, L"Sort", &vSortField, 1, nullptr);
    SysFreeString(vSortField.bstrVal);

    // Restrict to date range
    std::wstring filter = L"[Start] >= '" +
        std::wstring(from.Format("%m/%d/%Y").wc_str()) + L" 12:00 AM' AND " +
        L"[End] <= '" +
        std::wstring(to.Format("%m/%d/%Y").wc_str()) + L" 11:59 PM'";

    VARIANT vFilter; VariantInit(&vFilter);
    vFilter.vt = VT_BSTR;
    vFilter.bstrVal = SysAllocString(filter.c_str());
    VARIANT vFiltered;
    ComCallMethod(pItems, L"Restrict", &vFilter, 1, &vFiltered);
    SysFreeString(vFilter.bstrVal);

    IDispatch* pFiltered = (vFiltered.vt == VT_DISPATCH) ? vFiltered.pdispVal : nullptr;
    if (!pFiltered) pFiltered = pItems;

    // Count
    VARIANT vCount;
    ComGetProperty(pFiltered, L"Count", &vCount);
    int count = (vCount.vt == VT_I4) ? vCount.lVal : 0;

    for (int i = 1; i <= count; ++i) {
        VARIANT vIdx; VariantInit(&vIdx); vIdx.vt = VT_I4; vIdx.lVal = i;
        VARIANT vItem;
        ComCallMethod(pFiltered, L"Item", &vIdx, 1, &vItem);
        IDispatch* pItem = (vItem.vt == VT_DISPATCH) ? vItem.pdispVal : nullptr;
        if (!pItem) continue;

        CalendarEvent ev;

        VARIANT vSubj; ComGetProperty(pItem, L"Subject", &vSubj);
        ev.title = WideToUtf8(VariantToWString(vSubj)); VariantClear(&vSubj);

        VARIANT vBody; ComGetProperty(pItem, L"Body", &vBody);
        ev.description = WideToUtf8(VariantToWString(vBody)); VariantClear(&vBody);

        VARIANT vLoc; ComGetProperty(pItem, L"Location", &vLoc);
        ev.location = WideToUtf8(VariantToWString(vLoc)); VariantClear(&vLoc);

        VARIANT vStart; ComGetProperty(pItem, L"Start", &vStart);
        VARIANT vStartStr; VariantInit(&vStartStr);
        VariantChangeType(&vStartStr, &vStart, 0, VT_BSTR);
        ev.start = ParseOutlookDate(VariantToWString(vStartStr));
        VariantClear(&vStart); VariantClear(&vStartStr);

        VARIANT vEnd; ComGetProperty(pItem, L"End", &vEnd);
        VARIANT vEndStr; VariantInit(&vEndStr);
        VariantChangeType(&vEndStr, &vEnd, 0, VT_BSTR);
        ev.end = ParseOutlookDate(VariantToWString(vEndStr));
        VariantClear(&vEnd); VariantClear(&vEndStr);

        VARIANT vAllDay; ComGetProperty(pItem, L"AllDayEvent", &vAllDay);
        ev.allDay = (vAllDay.vt == VT_BOOL && vAllDay.boolVal == VARIANT_TRUE);
        VariantClear(&vAllDay);

        ev.color          = wxColour(26, 115, 232);  // default Google blue
        ev.calendarSource = "outlook";

        if (!ev.title.empty()) results.push_back(ev);
        pItem->Release();
    }

    if (pFiltered != pItems) pFiltered->Release();
    pItems->Release();
    pCalFolder->Release();
    pNS->Release();
    pApp->Release();

    return results;
}

std::string OutlookComConnector::PushEvent(const CalendarEvent& ev,
                                            std::string& errorMsg)
{
    errorMsg = "Push to Outlook via COM: not yet implemented.";
    return "";
}

#endif // _WIN32
