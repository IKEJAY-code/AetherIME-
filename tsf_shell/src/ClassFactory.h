#pragma once

#include <windows.h>

#include <unknwn.h>

class ClassFactory final : public IClassFactory {
 public:
  ClassFactory();

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // IClassFactory
  STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj) override;
  STDMETHODIMP LockServer(BOOL fLock) override;

 private:
  ~ClassFactory();

  LONG refCount_ = 1;
};

