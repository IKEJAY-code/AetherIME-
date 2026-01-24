#pragma once

#include <msctf.h>
#include <windows.h>

class DisplayAttributeInfo final : public ITfDisplayAttributeInfo {
 public:
  explicit DisplayAttributeInfo(const GUID& guid);

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // ITfDisplayAttributeInfo
  STDMETHODIMP GetGUID(GUID* pguid) override;
  STDMETHODIMP GetDescription(BSTR* pbstrDesc) override;
  STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE* pda) override;
  STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE* pda) override;
  STDMETHODIMP Reset() override;

 private:
  ~DisplayAttributeInfo();

  LONG refCount_ = 1;
  GUID guid_;
};

class EnumDisplayAttributeInfo final : public IEnumTfDisplayAttributeInfo {
 public:
  explicit EnumDisplayAttributeInfo(ITfDisplayAttributeInfo* info);

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // IEnumTfDisplayAttributeInfo
  STDMETHODIMP Clone(IEnumTfDisplayAttributeInfo** ppEnum) override;
  STDMETHODIMP Next(ULONG ulCount, ITfDisplayAttributeInfo** rgInfo, ULONG* pcFetched) override;
  STDMETHODIMP Reset() override;
  STDMETHODIMP Skip(ULONG ulCount) override;

 private:
  ~EnumDisplayAttributeInfo();

  LONG refCount_ = 1;
  ITfDisplayAttributeInfo* info_ = nullptr;
  bool used_ = false;
};

