#include "ClassFactory.h"

#include "Globals.h"
#include "TextService.h"

#include <new>

ClassFactory::ClassFactory() { DllAddRef(); }

ClassFactory::~ClassFactory() { DllRelease(); }

STDMETHODIMP ClassFactory::QueryInterface(REFIID riid, void** ppvObj) {
  if (!ppvObj) {
    return E_INVALIDARG;
  }
  *ppvObj = nullptr;

  if (riid == IID_IUnknown || riid == IID_IClassFactory) {
    *ppvObj = static_cast<IClassFactory*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ClassFactory::AddRef() { return static_cast<ULONG>(InterlockedIncrement(&refCount_)); }

STDMETHODIMP_(ULONG) ClassFactory::Release() {
  ULONG c = static_cast<ULONG>(InterlockedDecrement(&refCount_));
  if (c == 0) {
    delete this;
  }
  return c;
}

STDMETHODIMP ClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj) {
  if (!ppvObj) {
    return E_INVALIDARG;
  }
  *ppvObj = nullptr;

  if (pUnkOuter) {
    return CLASS_E_NOAGGREGATION;
  }

  auto* service = new (std::nothrow) TextService();
  if (!service) {
    return E_OUTOFMEMORY;
  }

  HRESULT hr = service->QueryInterface(riid, ppvObj);
  service->Release();
  return hr;
}

STDMETHODIMP ClassFactory::LockServer(BOOL fLock) {
  if (fLock) {
    DllAddRef();
  } else {
    DllRelease();
  }
  return S_OK;
}
