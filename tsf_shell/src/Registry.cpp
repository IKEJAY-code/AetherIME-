#include "Registry.h"

#include "ComPtr.h"
#include "Globals.h"
#include "Log.h"
#include "shurufa/Guids.h"

#include <msctf.h>
#include <strsafe.h>

#include <string>

namespace {

std::wstring GuidToString(REFGUID guid) {
  wchar_t buf[64];
  buf[0] = L'\0';
  StringFromGUID2(guid, buf, _countof(buf));
  return std::wstring(buf);
}

HRESULT SetRegSz(HKEY key, const wchar_t* valueName, const wchar_t* value) {
  return (RegSetValueExW(key, valueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(value),
                         static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t))) == ERROR_SUCCESS)
             ? S_OK
             : E_FAIL;
}

HRESULT RegisterComInprocServer(const CLSID& clsid, const wchar_t* friendlyName) {
  wchar_t modulePath[MAX_PATH];
  DWORD n = GetModuleFileNameW(g_hInstance, modulePath, _countof(modulePath));
  if (n == 0 || n >= _countof(modulePath)) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  std::wstring clsidStr = GuidToString(clsid);
  std::wstring clsidKeyPath = std::wstring(L"CLSID\\") + clsidStr;

  HKEY hClsid = nullptr;
  if (RegCreateKeyExW(HKEY_CLASSES_ROOT, clsidKeyPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hClsid,
                      nullptr) != ERROR_SUCCESS) {
    return E_FAIL;
  }
  SetRegSz(hClsid, nullptr, friendlyName);

  HKEY hInproc = nullptr;
  if (RegCreateKeyExW(hClsid, L"InprocServer32", 0, nullptr, 0, KEY_WRITE, nullptr, &hInproc, nullptr) !=
      ERROR_SUCCESS) {
    RegCloseKey(hClsid);
    return E_FAIL;
  }

  SetRegSz(hInproc, nullptr, modulePath);
  SetRegSz(hInproc, L"ThreadingModel", L"Apartment");

  RegCloseKey(hInproc);
  RegCloseKey(hClsid);
  return S_OK;
}

HRESULT UnregisterComInprocServer(const CLSID& clsid) {
  std::wstring clsidStr = GuidToString(clsid);
  std::wstring clsidKeyPath = std::wstring(L"CLSID\\") + clsidStr;
  LONG r = RegDeleteTreeW(HKEY_CLASSES_ROOT, clsidKeyPath.c_str());
  if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
    return HRESULT_FROM_WIN32(r);
  }
  return S_OK;
}

} // namespace

HRESULT RegisterTsfServer() {
  shurufa::log::Info(L"RegisterTsfServer");

  HRESULT hr = RegisterComInprocServer(CLSID_ShurufaTextService, L"Shurufa Ghost Text TIP");
  if (FAILED(hr)) {
    shurufa::log::Error(L"RegisterComInprocServer failed hr=0x%08X", hr);
    return hr;
  }

  hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  const bool coInit = SUCCEEDED(hr);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
    shurufa::log::Error(L"CoInitializeEx failed hr=0x%08X", hr);
    return hr;
  }

  const LANGID langid = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
  const wchar_t* desc = L"Shurufa Ghost Text (en-US)";

  ComPtr<ITfInputProcessorProfiles> profiles;
  hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles,
                        reinterpret_cast<void**>(profiles.put()));
  if (SUCCEEDED(hr)) {
    profiles->Register(CLSID_ShurufaTextService);

    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(g_hInstance, modulePath, _countof(modulePath));
    profiles->AddLanguageProfile(CLSID_ShurufaTextService, langid, GUID_PROFILE_ShurufaGhostEnUS, desc,
                                 static_cast<ULONG>(wcslen(desc)), modulePath,
                                 static_cast<ULONG>(wcslen(modulePath)), 0);
    profiles->EnableLanguageProfile(CLSID_ShurufaTextService, langid, GUID_PROFILE_ShurufaGhostEnUS, TRUE);
  } else {
    shurufa::log::Error(L"CoCreateInstance(InputProcessorProfiles) failed hr=0x%08X", hr);
  }

  ComPtr<ITfCategoryMgr> cat;
  hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                        reinterpret_cast<void**>(cat.put()));
  if (SUCCEEDED(hr)) {
    cat->RegisterCategory(CLSID_ShurufaTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_ShurufaTextService);
    cat->RegisterCategory(CLSID_ShurufaTextService, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, CLSID_ShurufaTextService);
  } else {
    shurufa::log::Error(L"CoCreateInstance(CategoryMgr) failed hr=0x%08X", hr);
  }

  if (coInit) {
    CoUninitialize();
  }
  return S_OK;
}

HRESULT UnregisterTsfServer() {
  shurufa::log::Info(L"UnregisterTsfServer");

  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  const bool coInit = SUCCEEDED(hr);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
    return hr;
  }

  const LANGID langid = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);

  ComPtr<ITfInputProcessorProfiles> profiles;
  hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles,
                        reinterpret_cast<void**>(profiles.put()));
  if (SUCCEEDED(hr)) {
    profiles->EnableLanguageProfile(CLSID_ShurufaTextService, langid, GUID_PROFILE_ShurufaGhostEnUS, FALSE);
    profiles->RemoveLanguageProfile(CLSID_ShurufaTextService, langid, GUID_PROFILE_ShurufaGhostEnUS);
    profiles->Unregister(CLSID_ShurufaTextService);
  }

  ComPtr<ITfCategoryMgr> cat;
  hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                        reinterpret_cast<void**>(cat.put()));
  if (SUCCEEDED(hr)) {
    cat->UnregisterCategory(CLSID_ShurufaTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_ShurufaTextService);
    cat->UnregisterCategory(CLSID_ShurufaTextService, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, CLSID_ShurufaTextService);
  }

  if (coInit) {
    CoUninitialize();
  }

  UnregisterComInprocServer(CLSID_ShurufaTextService);
  return S_OK;
}
