#include "DisplayAttribute.h"

#include "shurufa/Guids.h"

#include <new>
#include <oleauto.h>

DisplayAttributeInfo::DisplayAttributeInfo(const GUID& guid) : guid_(guid) {}

DisplayAttributeInfo::~DisplayAttributeInfo() = default;

STDMETHODIMP DisplayAttributeInfo::QueryInterface(REFIID riid, void** ppvObj) {
  if (!ppvObj) {
    return E_INVALIDARG;
  }
  *ppvObj = nullptr;

  if (riid == IID_IUnknown || riid == IID_ITfDisplayAttributeInfo) {
    *ppvObj = static_cast<ITfDisplayAttributeInfo*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) DisplayAttributeInfo::AddRef() { return static_cast<ULONG>(InterlockedIncrement(&refCount_)); }

STDMETHODIMP_(ULONG) DisplayAttributeInfo::Release() {
  ULONG c = static_cast<ULONG>(InterlockedDecrement(&refCount_));
  if (c == 0) {
    delete this;
  }
  return c;
}

STDMETHODIMP DisplayAttributeInfo::GetGUID(GUID* pguid) {
  if (!pguid) {
    return E_INVALIDARG;
  }
  *pguid = guid_;
  return S_OK;
}

STDMETHODIMP DisplayAttributeInfo::GetDescription(BSTR* pbstrDesc) {
  if (!pbstrDesc) {
    return E_INVALIDARG;
  }
  *pbstrDesc = SysAllocString(L"Shurufa Ghost Text");
  return *pbstrDesc ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP DisplayAttributeInfo::GetAttributeInfo(TF_DISPLAYATTRIBUTE* pda) {
  if (!pda) {
    return E_INVALIDARG;
  }
  TF_DISPLAYATTRIBUTE da = {};
  da.crText.type = TF_CT_COLORREF;
  da.crText.cr = RGB(160, 160, 160);
  da.crBk.type = TF_CT_NONE;
  da.lsStyle = TF_LS_NONE;
  da.fBoldLine = FALSE;
  da.crLine.type = TF_CT_NONE;
  *pda = da;
  return S_OK;
}

STDMETHODIMP DisplayAttributeInfo::SetAttributeInfo(const TF_DISPLAYATTRIBUTE* /*pda*/) { return E_NOTIMPL; }

STDMETHODIMP DisplayAttributeInfo::Reset() { return S_OK; }

EnumDisplayAttributeInfo::EnumDisplayAttributeInfo(ITfDisplayAttributeInfo* info) : info_(info) {
  if (info_) {
    info_->AddRef();
  }
}

EnumDisplayAttributeInfo::~EnumDisplayAttributeInfo() {
  if (info_) {
    info_->Release();
    info_ = nullptr;
  }
}

STDMETHODIMP EnumDisplayAttributeInfo::QueryInterface(REFIID riid, void** ppvObj) {
  if (!ppvObj) {
    return E_INVALIDARG;
  }
  *ppvObj = nullptr;

  if (riid == IID_IUnknown || riid == IID_IEnumTfDisplayAttributeInfo) {
    *ppvObj = static_cast<IEnumTfDisplayAttributeInfo*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) EnumDisplayAttributeInfo::AddRef() { return static_cast<ULONG>(InterlockedIncrement(&refCount_)); }

STDMETHODIMP_(ULONG) EnumDisplayAttributeInfo::Release() {
  ULONG c = static_cast<ULONG>(InterlockedDecrement(&refCount_));
  if (c == 0) {
    delete this;
  }
  return c;
}

STDMETHODIMP EnumDisplayAttributeInfo::Clone(IEnumTfDisplayAttributeInfo** ppEnum) {
  if (!ppEnum) {
    return E_INVALIDARG;
  }
  *ppEnum = new (std::nothrow) EnumDisplayAttributeInfo(info_);
  if (!*ppEnum) {
    return E_OUTOFMEMORY;
  }
  if (used_) {
    (*ppEnum)->Skip(1);
  }
  return S_OK;
}

STDMETHODIMP EnumDisplayAttributeInfo::Next(ULONG ulCount, ITfDisplayAttributeInfo** rgInfo, ULONG* pcFetched) {
  if (!rgInfo || ulCount == 0) {
    return E_INVALIDARG;
  }
  if (pcFetched) {
    *pcFetched = 0;
  }

  if (used_ || !info_) {
    return S_FALSE;
  }

  if (ulCount >= 1) {
    rgInfo[0] = info_;
    info_->AddRef();
    used_ = true;
    if (pcFetched) {
      *pcFetched = 1;
    }
    return S_OK;
  }
  return S_FALSE;
}

STDMETHODIMP EnumDisplayAttributeInfo::Reset() {
  used_ = false;
  return S_OK;
}

STDMETHODIMP EnumDisplayAttributeInfo::Skip(ULONG ulCount) {
  if (ulCount == 0) {
    return S_OK;
  }
  used_ = true;
  return S_FALSE;
}
