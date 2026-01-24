#include "ClassFactory.h"
#include "Globals.h"
#include "Log.h"
#include "Registry.h"
#include "shurufa/Guids.h"

#include <windows.h>

#include <new>

HINSTANCE g_hInstance = nullptr;
LONG g_dllRefCount = 0;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*reserved*/) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_hInstance = hModule;
    DisableThreadLibraryCalls(hModule);
  }
  return TRUE;
}

STDAPI DllCanUnloadNow(void) {
  return (g_dllRefCount == 0) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
  if (!ppv) {
    return E_INVALIDARG;
  }
  *ppv = nullptr;

  if (rclsid != CLSID_ShurufaTextService) {
    return CLASS_E_CLASSNOTAVAILABLE;
  }

  auto* factory = new (std::nothrow) ClassFactory();
  if (!factory) {
    return E_OUTOFMEMORY;
  }
  HRESULT hr = factory->QueryInterface(riid, ppv);
  factory->Release();
  return hr;
}

STDAPI DllRegisterServer(void) { return RegisterTsfServer(); }

STDAPI DllUnregisterServer(void) { return UnregisterTsfServer(); }
