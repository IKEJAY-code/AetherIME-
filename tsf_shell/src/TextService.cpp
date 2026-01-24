#include "TextService.h"

#include "ComPtr.h"
#include "DisplayAttribute.h"
#include "Globals.h"
#include "Log.h"
#include "shurufa/Guids.h"

#include <inputscope.h>
#include <msctf.h>
#include <oleauto.h>

#include <cstdlib>
#include <atomic>
#include <memory>
#include <new>

namespace {

constexpr UINT WM_SHURUFA_SUGGESTION = WM_APP + 0x521;
constexpr UINT WM_SHURUFA_CLEAR_GHOST = WM_APP + 0x522;
constexpr UINT TIMER_ID_DEBOUNCE = 1;
constexpr UINT DEBOUNCE_MS = 60;
constexpr uint32_t MAX_CONTEXT_BEFORE = 256;

struct IgnoreTextEditsScope {
  std::atomic<long>& depth;
  explicit IgnoreTextEditsScope(std::atomic<long>& d) : depth(d) { depth.fetch_add(1); }
  ~IgnoreTextEditsScope() { depth.fetch_sub(1); }
};

bool IsModifierKey(WPARAM wParam) {
  switch (wParam) {
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
      return true;
    default:
      return false;
  }
}

bool IsNavigationOrEditKey(WPARAM wParam) {
  switch (wParam) {
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_BACK:
    case VK_DELETE:
      return true;
    default:
      return false;
  }
}

std::string EnvStringOrDefault(const char* name, const std::string& fallback) {
  if (const char* val = std::getenv(name)) {
    if (*val != '\0') {
      return std::string(val);
    }
  }
  return fallback;
}

uint16_t EnvPortOrDefault(const char* name, uint16_t fallback) {
  if (const char* val = std::getenv(name)) {
    char* end = nullptr;
    unsigned long p = strtoul(val, &end, 10);
    if (end != val && p > 0 && p <= 65535) {
      return static_cast<uint16_t>(p);
    }
  }
  return fallback;
}

} // namespace

// ---------------- Edit sessions ----------------

class ClearGhostEditSession final : public ITfEditSession {
 public:
  ClearGhostEditSession(TextService* svc, ITfContext* ctx) : svc_(svc), ctx_(ctx) {
    svc_->AddRef();
    if (ctx_) {
      ctx_->AddRef();
    }
  }
  ~ClearGhostEditSession() {
    if (ctx_) {
      ctx_->Release();
    }
    svc_->Release();
  }

  STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (riid == IID_IUnknown || riid == IID_ITfEditSession) {
      *ppvObj = static_cast<ITfEditSession*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&ref_)); }
  STDMETHODIMP_(ULONG) Release() override {
    ULONG c = static_cast<ULONG>(InterlockedDecrement(&ref_));
    if (c == 0) delete this;
    return c;
  }

  STDMETHODIMP DoEditSession(TfEditCookie ec) override { return svc_->ClearGhostInEditSession(ctx_, ec); }

 private:
  LONG ref_ = 1;
  TextService* svc_ = nullptr;
  ITfContext* ctx_ = nullptr;
};

class ShowGhostEditSession final : public ITfEditSession {
 public:
  ShowGhostEditSession(TextService* svc, ITfContext* ctx, std::wstring suggestion)
      : svc_(svc), ctx_(ctx), suggestion_(std::move(suggestion)) {
    svc_->AddRef();
    if (ctx_) {
      ctx_->AddRef();
    }
  }
  ~ShowGhostEditSession() {
    if (ctx_) {
      ctx_->Release();
    }
    svc_->Release();
  }

  STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (riid == IID_IUnknown || riid == IID_ITfEditSession) {
      *ppvObj = static_cast<ITfEditSession*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&ref_)); }
  STDMETHODIMP_(ULONG) Release() override {
    ULONG c = static_cast<ULONG>(InterlockedDecrement(&ref_));
    if (c == 0) delete this;
    return c;
  }

  STDMETHODIMP DoEditSession(TfEditCookie ec) override { return svc_->ShowGhostInEditSession(ctx_, ec, suggestion_); }

 private:
  LONG ref_ = 1;
  TextService* svc_ = nullptr;
  ITfContext* ctx_ = nullptr;
  std::wstring suggestion_;
};

class AcceptGhostEditSession final : public ITfEditSession {
 public:
  AcceptGhostEditSession(TextService* svc, ITfContext* ctx) : svc_(svc), ctx_(ctx) {
    svc_->AddRef();
    if (ctx_) {
      ctx_->AddRef();
    }
  }
  ~AcceptGhostEditSession() {
    if (ctx_) {
      ctx_->Release();
    }
    svc_->Release();
  }

  STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (riid == IID_IUnknown || riid == IID_ITfEditSession) {
      *ppvObj = static_cast<ITfEditSession*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&ref_)); }
  STDMETHODIMP_(ULONG) Release() override {
    ULONG c = static_cast<ULONG>(InterlockedDecrement(&ref_));
    if (c == 0) delete this;
    return c;
  }

  STDMETHODIMP DoEditSession(TfEditCookie ec) override { return svc_->AcceptGhostInEditSession(ctx_, ec); }

 private:
  LONG ref_ = 1;
  TextService* svc_ = nullptr;
  ITfContext* ctx_ = nullptr;
};

// ---------------- TextService ----------------

TextService::TextService() { DllAddRef(); }

TextService::~TextService() {
  DestroyMessageWindow();
  if (ghost_composition_) {
    ghost_composition_->Release();
    ghost_composition_ = nullptr;
  }
  if (focus_context_) {
    focus_context_->Release();
    focus_context_ = nullptr;
  }
  if (thread_mgr_) {
    thread_mgr_->Release();
    thread_mgr_ = nullptr;
  }
  DllRelease();
}

STDMETHODIMP TextService::QueryInterface(REFIID riid, void** ppvObj) {
  if (!ppvObj) {
    return E_INVALIDARG;
  }
  *ppvObj = nullptr;

  if (riid == IID_IUnknown || riid == IID_ITfTextInputProcessor) {
    *ppvObj = static_cast<ITfTextInputProcessor*>(this);
  } else if (riid == IID_ITfKeyEventSink) {
    *ppvObj = static_cast<ITfKeyEventSink*>(this);
  } else if (riid == IID_ITfThreadMgrEventSink) {
    *ppvObj = static_cast<ITfThreadMgrEventSink*>(this);
  } else if (riid == IID_ITfTextEditSink) {
    *ppvObj = static_cast<ITfTextEditSink*>(this);
  } else if (riid == IID_ITfCompositionSink) {
    *ppvObj = static_cast<ITfCompositionSink*>(this);
  } else if (riid == IID_ITfDisplayAttributeProvider) {
    *ppvObj = static_cast<ITfDisplayAttributeProvider*>(this);
  } else {
    return E_NOINTERFACE;
  }

  AddRef();
  return S_OK;
}

STDMETHODIMP_(ULONG) TextService::AddRef() { return static_cast<ULONG>(InterlockedIncrement(&refCount_)); }

STDMETHODIMP_(ULONG) TextService::Release() {
  ULONG c = static_cast<ULONG>(InterlockedDecrement(&refCount_));
  if (c == 0) {
    delete this;
  }
  return c;
}

STDMETHODIMP TextService::Activate(ITfThreadMgr* ptim, TfClientId tid) {
  if (!ptim) {
    return E_INVALIDARG;
  }

  shurufa::log::Info(L"TextService::Activate tid=%lu", static_cast<unsigned long>(tid));

  thread_mgr_ = ptim;
  thread_mgr_->AddRef();
  client_id_ = tid;

  // Register display attribute GUID -> atom
  {
    ComPtr<ITfCategoryMgr> cat;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                                   reinterpret_cast<void**>(cat.put())))) {
      cat->RegisterGUID(GUID_DISPLAYATTRIBUTE_GHOST, &ghost_display_attr_atom_);
    }
  }

  // Message window (TSF thread) for IPC callbacks and debounce timer.
  CreateMessageWindow();

  // Start IPC client (background thread); callback posts message back to TSF thread.
  std::string host = EnvStringOrDefault("SHURUFA_ENGINE_HOST", "127.0.0.1");
  uint16_t port = EnvPortOrDefault("SHURUFA_ENGINE_PORT", 48080);
  shurufa::log::Info(L"IPC endpoint %S:%u", host.c_str(), port);
  ipc_.SetEndpoint(host, port);
  ipc_.Start([this](const SuggestionResponse& rsp) { PostSuggestionToTsfThread(rsp); });

  // Key event sink
  {
    ComPtr<ITfKeystrokeMgr> kmgr;
    if (SUCCEEDED(thread_mgr_->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(kmgr.put())))) {
      kmgr->AdviseKeyEventSink(client_id_, static_cast<ITfKeyEventSink*>(this), TRUE);
    }
  }

  // ThreadMgr event sink
  {
    ComPtr<ITfSource> src;
    if (SUCCEEDED(thread_mgr_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(src.put())))) {
      src->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this), &cookie_threadmgr_sink_);
    }
  }

  // Initialize focus
  {
    ITfDocumentMgr* docMgr = nullptr;
    if (SUCCEEDED(thread_mgr_->GetFocus(&docMgr)) && docMgr) {
      OnSetFocus(docMgr, nullptr);
      docMgr->Release();
    }
  }

  return S_OK;
}

STDMETHODIMP TextService::Deactivate() {
  shurufa::log::Info(L"TextService::Deactivate");

  ipc_.Stop();

  CancelDebounce();

  // Unadvise thread mgr sink
  if (thread_mgr_ && cookie_threadmgr_sink_ != TF_INVALID_COOKIE) {
    ComPtr<ITfSource> src;
    if (SUCCEEDED(thread_mgr_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(src.put())))) {
      src->UnadviseSink(cookie_threadmgr_sink_);
    }
    cookie_threadmgr_sink_ = TF_INVALID_COOKIE;
  }

  // Unadvise key event sink
  if (thread_mgr_) {
    ComPtr<ITfKeystrokeMgr> kmgr;
    if (SUCCEEDED(thread_mgr_->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(kmgr.put())))) {
      kmgr->UnadviseKeyEventSink(client_id_);
    }
  }

  // Unadvise text edit sink
  if (focus_context_ && cookie_textedit_sink_ != TF_INVALID_COOKIE) {
    ComPtr<ITfSource> src;
    if (SUCCEEDED(focus_context_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(src.put())))) {
      src->UnadviseSink(cookie_textedit_sink_);
    }
    cookie_textedit_sink_ = TF_INVALID_COOKIE;
  }

  // Best-effort clear ghost on current focus context.
  if (focus_context_) {
    RequestClearGhost(focus_context_);
  }

  if (focus_context_) {
    focus_context_->Release();
    focus_context_ = nullptr;
  }
  if (thread_mgr_) {
    thread_mgr_->Release();
    thread_mgr_ = nullptr;
  }
  client_id_ = TF_CLIENTID_NULL;

  DestroyMessageWindow();

  return S_OK;
}

// ---------------- ITfKeyEventSink ----------------

STDMETHODIMP TextService::OnSetFocus(BOOL /*fForeground*/) { return S_OK; }

STDMETHODIMP TextService::OnTestKeyDown(ITfContext* /*pic*/, WPARAM wParam, LPARAM /*lParam*/, BOOL* pfEaten) {
  if (!pfEaten) return E_INVALIDARG;
  *pfEaten = FALSE;
  if (wParam == VK_TAB && HasGhost()) {
    *pfEaten = TRUE;
  } else if (wParam == VK_ESCAPE && HasGhost()) {
    *pfEaten = TRUE;
  }
  return S_OK;
}

STDMETHODIMP TextService::OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
  return OnTestKeyDown(pic, wParam, lParam, pfEaten);
}

STDMETHODIMP TextService::OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM /*lParam*/, BOOL* pfEaten) {
  if (!pfEaten) return E_INVALIDARG;
  *pfEaten = FALSE;

  if (!pic) {
    return S_OK;
  }

  if (wParam == VK_TAB && HasGhost()) {
    *pfEaten = TRUE;
    RequestAcceptGhost(pic);
    return S_OK;
  }
  if (wParam == VK_ESCAPE && HasGhost()) {
    *pfEaten = TRUE;
    RequestClearGhost(pic);
    return S_OK;
  }

  if (HasGhost()) {
    // 用户继续输入/移动光标时先清理 ghost（不吃键，保留原生行为）。
    if (!IsModifierKey(wParam)) {
      RequestClearGhost(pic);
    }
  }

  // 默认不吃键，让应用保留原生输入行为（Phase 1 仅叠加 ghost）。
  return S_OK;
}

STDMETHODIMP TextService::OnKeyUp(ITfContext* /*pic*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL* pfEaten) {
  if (!pfEaten) return E_INVALIDARG;
  *pfEaten = FALSE;
  return S_OK;
}

STDMETHODIMP TextService::OnPreservedKey(ITfContext* /*pic*/, REFGUID /*rguid*/, BOOL* pfEaten) {
  if (!pfEaten) return E_INVALIDARG;
  *pfEaten = FALSE;
  return S_OK;
}

// ---------------- ITfThreadMgrEventSink ----------------

STDMETHODIMP TextService::OnSetFocus(ITfDocumentMgr* pDocMgrFocus, ITfDocumentMgr* /*pDocMgrPrevFocus*/) {
  shurufa::log::Info(L"ThreadMgr OnSetFocus");

  // Cancel pending/in-flight requests on focus change.
  CancelDebounce();
  if (!inflight_request_id_.empty()) {
    ipc_.SendCancel(inflight_request_id_);
    inflight_request_id_.clear();
  }

  // Unadvise previous text edit sink.
  if (focus_context_ && cookie_textedit_sink_ != TF_INVALID_COOKIE) {
    ComPtr<ITfSource> src;
    if (SUCCEEDED(focus_context_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(src.put())))) {
      src->UnadviseSink(cookie_textedit_sink_);
    }
    cookie_textedit_sink_ = TF_INVALID_COOKIE;
  }

  if (focus_context_) {
    // Best-effort clear ghost in old context.
    RequestClearGhost(focus_context_);
    focus_context_->Release();
    focus_context_ = nullptr;
  }

  if (!pDocMgrFocus) {
    return S_OK;
  }

  ITfContext* ctx = nullptr;
  if (FAILED(pDocMgrFocus->GetTop(&ctx)) || !ctx) {
    return S_OK;
  }
  focus_context_ = ctx;  // already AddRef'd by GetTop

  // Advise text edit sink for new focus context.
  {
    ComPtr<ITfSource> src;
    if (SUCCEEDED(focus_context_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(src.put())))) {
      src->AdviseSink(IID_ITfTextEditSink, static_cast<ITfTextEditSink*>(this), &cookie_textedit_sink_);
    }
  }

  return S_OK;
}

// ---------------- ITfTextEditSink ----------------

STDMETHODIMP TextService::OnEndEdit(ITfContext* pic, TfEditCookie ecReadOnly, ITfEditRecord* /*pEditRecord*/) {
  if (!pic) {
    return S_OK;
  }
  if (ignore_text_edits_depth_.load() > 0) {
    return S_OK;
  }

  if (IsSensitiveInput(pic, ecReadOnly)) {
    if (HasGhost()) {
      if (msg_hwnd_) {
        PostMessageW(msg_hwnd_, WM_SHURUFA_CLEAR_GHOST, 0, 0);
      }
    }
    return S_OK;
  }

  // Only trigger on insertion point (no selection).
  TF_SELECTION sel = {};
  ULONG fetched = 0;
  if (FAILED(pic->GetSelection(ecReadOnly, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) || fetched == 0) {
    return S_OK;
  }
  BOOL empty = FALSE;
  if (FAILED(sel.range->IsEmpty(ecReadOnly, &empty)) || !empty) {
    sel.range->Release();
    if (HasGhost()) {
      if (msg_hwnd_) {
        PostMessageW(msg_hwnd_, WM_SHURUFA_CLEAR_GHOST, 0, 0);
      }
    }
    return S_OK;
  }
  sel.range->Release();

  // If doc changes not caused by us, clear existing ghost first.
  if (HasGhost()) {
    if (msg_hwnd_) {
      PostMessageW(msg_hwnd_, WM_SHURUFA_CLEAR_GHOST, 0, 0);
    }
  }

  std::wstring before;
  if (!ReadContextBeforeCursor(pic, ecReadOnly, &before, MAX_CONTEXT_BEFORE)) {
    return S_OK;
  }

  pending_context_ = std::move(before);
  pending_cursor_utf16_ = static_cast<uint32_t>(pending_context_.size());
  ScheduleDebounce();

  return S_OK;
}

// ---------------- ITfCompositionSink ----------------

STDMETHODIMP TextService::OnCompositionTerminated(TfEditCookie /*ecWrite*/, ITfComposition* pComposition) {
  if (ghost_composition_ && pComposition == ghost_composition_) {
    ghost_composition_->Release();
    ghost_composition_ = nullptr;
  }
  return S_OK;
}

// ---------------- DisplayAttributeProvider ----------------

STDMETHODIMP TextService::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** ppEnum) {
  if (!ppEnum) return E_INVALIDARG;
  *ppEnum = nullptr;

  auto* info = new (std::nothrow) DisplayAttributeInfo(GUID_DISPLAYATTRIBUTE_GHOST);
  if (!info) {
    return E_OUTOFMEMORY;
  }
  auto* e = new (std::nothrow) ::EnumDisplayAttributeInfo(info);
  info->Release();
  if (!e) {
    return E_OUTOFMEMORY;
  }
  *ppEnum = e;
  return S_OK;
}

STDMETHODIMP TextService::GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** ppInfo) {
  if (!ppInfo) return E_INVALIDARG;
  *ppInfo = nullptr;
  if (guid != GUID_DISPLAYATTRIBUTE_GHOST) {
    return E_INVALIDARG;
  }
  auto* info = new (std::nothrow) DisplayAttributeInfo(guid);
  if (!info) {
    return E_OUTOFMEMORY;
  }
  *ppInfo = info;
  return S_OK;
}

// ---------------- Message window ----------------

LRESULT CALLBACK TextService::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  auto* svc = reinterpret_cast<TextService*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
  if (!svc) {
    return DefWindowProcW(hwnd, msg, wParam, lParam);
  }

  if (msg == WM_SHURUFA_SUGGESTION) {
    auto* payload = reinterpret_cast<SuggestionPayload*>(lParam);
    svc->HandleSuggestionOnTsfThread(payload);
    return 0;
  }
  if (msg == WM_SHURUFA_CLEAR_GHOST) {
    if (svc->focus_context_) {
      svc->RequestClearGhost(svc->focus_context_);
    }
    return 0;
  }
  if (msg == WM_TIMER && wParam == TIMER_ID_DEBOUNCE) {
    svc->SendPendingRequest();
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

HRESULT TextService::CreateMessageWindow() {
  if (msg_hwnd_) {
    return S_OK;
  }

  const wchar_t* kClassName = L"ShurufaTsfMsgWindow";

  static std::atomic<bool> s_registered{false};
  if (!s_registered.exchange(true)) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &TextService::WndProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
  }

  msg_hwnd_ = CreateWindowExW(0, kClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, g_hInstance, this);
  if (!msg_hwnd_) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  return S_OK;
}

void TextService::DestroyMessageWindow() {
  CancelDebounce();
  if (msg_hwnd_) {
    DestroyWindow(msg_hwnd_);
    msg_hwnd_ = nullptr;
  }
}

void TextService::ScheduleDebounce() {
  if (!msg_hwnd_) {
    return;
  }
  KillTimer(msg_hwnd_, TIMER_ID_DEBOUNCE);
  debounce_timer_id_ = SetTimer(msg_hwnd_, TIMER_ID_DEBOUNCE, DEBOUNCE_MS, nullptr);
}

void TextService::CancelDebounce() {
  if (msg_hwnd_) {
    KillTimer(msg_hwnd_, TIMER_ID_DEBOUNCE);
  }
  debounce_timer_id_ = 0;
}

void TextService::SendPendingRequest() {
  CancelDebounce();
  if (!focus_context_) {
    return;
  }

  // Cancel old in-flight.
  if (!inflight_request_id_.empty()) {
    ipc_.SendCancel(inflight_request_id_);
  }

  inflight_request_id_ = std::to_wstring(next_request_id_.fetch_add(1));

  SuggestRequest req;
  req.request_id = inflight_request_id_;
  req.context = pending_context_;
  req.cursor_utf16 = pending_cursor_utf16_;
  req.max_len = 32;
  req.language_hint = L"auto";

  ipc_.SendSuggest(req);
}

void TextService::PostSuggestionToTsfThread(const SuggestionResponse& rsp) {
  if (!msg_hwnd_) {
    return;
  }

  auto* payload = new (std::nothrow) SuggestionPayload();
  if (!payload) {
    return;
  }
  payload->request_id = rsp.request_id;
  payload->suggestion = rsp.suggestion;
  payload->confidence = rsp.confidence;
  payload->replace_start_utf16 = rsp.replace_start_utf16;
  payload->replace_end_utf16 = rsp.replace_end_utf16;

  PostMessageW(msg_hwnd_, WM_SHURUFA_SUGGESTION, 0, reinterpret_cast<LPARAM>(payload));
}

void TextService::HandleSuggestionOnTsfThread(SuggestionPayload* payload) {
  std::unique_ptr<SuggestionPayload> guard(payload);
  if (!payload) {
    return;
  }
  if (payload->request_id != inflight_request_id_) {
    return; // late result
  }

  if (!focus_context_) {
    return;
  }

  if (payload->suggestion.empty() || payload->confidence < 0.50f) {
    RequestClearGhost(focus_context_);
    return;
  }

  // Phase 1: ignore replace_range, treat as insertion at cursor.
  RequestShowGhost(focus_context_, payload->suggestion);
}

// ---------------- Context & security ----------------

bool TextService::IsSensitiveInput(ITfContext* pic, TfEditCookie /*ecReadOnly*/) {
  // Best-effort: if input scope says password, disable.
  ComPtr<ITfInputScope> scope;
  if (SUCCEEDED(pic->QueryInterface(IID_ITfInputScope, reinterpret_cast<void**>(scope.put())))) {
    InputScope* scopes = nullptr;
    UINT count = 0;
    if (SUCCEEDED(scope->GetInputScopes(&scopes, &count)) && scopes) {
      bool sensitive = false;
      for (UINT i = 0; i < count; ++i) {
        if (scopes[i] == IS_PASSWORD) {
          sensitive = true;
          break;
        }
      }
      CoTaskMemFree(scopes);
      return sensitive;
    }
  }
  return false;
}

bool TextService::ReadContextBeforeCursor(ITfContext* pic, TfEditCookie ecReadOnly, std::wstring* out_before,
                                         uint32_t max_chars) {
  if (!out_before) {
    return false;
  }

  TF_SELECTION sel = {};
  ULONG fetched = 0;
  if (FAILED(pic->GetSelection(ecReadOnly, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) || fetched == 0) {
    return false;
  }

  BOOL empty = FALSE;
  if (FAILED(sel.range->IsEmpty(ecReadOnly, &empty)) || !empty) {
    sel.range->Release();
    return false;
  }

  // caret at end anchor
  sel.range->Collapse(ecReadOnly, TF_ANCHOR_END);

  ComPtr<ITfRange> before;
  if (FAILED(sel.range->Clone(before.put()))) {
    sel.range->Release();
    return false;
  }
  sel.range->Release();

  LONG moved = 0;
  before->ShiftStart(ecReadOnly, -static_cast<LONG>(max_chars), &moved, nullptr);

  std::wstring text;
  text.resize(max_chars);
  ULONG got = 0;
  if (FAILED(before->GetText(ecReadOnly, 0, text.data(), max_chars, &got))) {
    return false;
  }
  text.resize(got);

  *out_before = std::move(text);
  return true;
}

// ---------------- Ghost operations ----------------

void TextService::RequestClearGhost(ITfContext* pic) {
  if (!pic) return;
  HRESULT hr = S_OK;
  auto* es = new (std::nothrow) ClearGhostEditSession(this, pic);
  if (!es) return;
  pic->RequestEditSession(client_id_, es, TF_ES_SYNC | TF_ES_READWRITE, &hr);
  es->Release();
}

void TextService::RequestShowGhost(ITfContext* pic, const std::wstring& suggestion) {
  if (!pic) return;
  HRESULT hr = S_OK;
  auto* es = new (std::nothrow) ShowGhostEditSession(this, pic, suggestion);
  if (!es) return;
  pic->RequestEditSession(client_id_, es, TF_ES_SYNC | TF_ES_READWRITE, &hr);
  es->Release();
}

void TextService::RequestAcceptGhost(ITfContext* pic) {
  if (!pic) return;
  HRESULT hr = S_OK;
  auto* es = new (std::nothrow) AcceptGhostEditSession(this, pic);
  if (!es) return;
  pic->RequestEditSession(client_id_, es, TF_ES_SYNC | TF_ES_READWRITE, &hr);
  es->Release();
}

HRESULT TextService::ClearGhostInEditSession(ITfContext* pic, TfEditCookie ecWrite) {
  IgnoreTextEditsScope ignore(ignore_text_edits_depth_);
  if (!pic || !ghost_composition_) {
    return S_OK;
  }

  ComPtr<ITfRange> range;
  if (FAILED(ghost_composition_->GetRange(range.put())) || !range.get()) {
    ghost_composition_->Release();
    ghost_composition_ = nullptr;
    return S_OK;
  }

  // Remove display attribute first (best-effort).
  ComPtr<ITfProperty> prop;
  if (SUCCEEDED(pic->GetProperty(GUID_PROP_ATTRIBUTE, prop.put())) && prop.get()) {
    prop->Clear(ecWrite, range.get());
  }

  range->SetText(ecWrite, 0, L"", 0);
  ghost_composition_->EndComposition(ecWrite);
  ghost_composition_->Release();
  ghost_composition_ = nullptr;
  return S_OK;
}

HRESULT TextService::ShowGhostInEditSession(ITfContext* pic, TfEditCookie ecWrite, const std::wstring& suggestion) {
  IgnoreTextEditsScope ignore(ignore_text_edits_depth_);
  if (!pic) {
    return E_INVALIDARG;
  }
  if (suggestion.empty()) {
    return ClearGhostInEditSession(pic, ecWrite);
  }

  // Clear existing
  if (ghost_composition_) {
    ClearGhostInEditSession(pic, ecWrite);
  }

  ComPtr<ITfContextComposition> cc;
  if (FAILED(pic->QueryInterface(IID_ITfContextComposition, reinterpret_cast<void**>(cc.put()))) || !cc.get()) {
    return E_NOINTERFACE;
  }

  TF_SELECTION sel = {};
  ULONG fetched = 0;
  if (FAILED(pic->GetSelection(ecWrite, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) || fetched == 0) {
    return E_FAIL;
  }

  // Start composition at caret.
  sel.range->Collapse(ecWrite, TF_ANCHOR_START);
  HRESULT hr = cc->StartComposition(ecWrite, sel.range, static_cast<ITfCompositionSink*>(this), &ghost_composition_);
  sel.range->Release();
  if (FAILED(hr) || !ghost_composition_) {
    return hr;
  }

  ComPtr<ITfRange> range;
  if (FAILED(ghost_composition_->GetRange(range.put())) || !range.get()) {
    ghost_composition_->Release();
    ghost_composition_ = nullptr;
    return E_FAIL;
  }

  range->SetText(ecWrite, 0, suggestion.c_str(), static_cast<LONG>(suggestion.size()));

  // Apply display attribute.
  if (ghost_display_attr_atom_ != TF_INVALID_GUIDATOM) {
    ComPtr<ITfProperty> prop;
    if (SUCCEEDED(pic->GetProperty(GUID_PROP_ATTRIBUTE, prop.put())) && prop.get()) {
      VARIANT var;
      VariantInit(&var);
      var.vt = VT_I4;
      var.lVal = ghost_display_attr_atom_;
      prop->SetValue(ecWrite, range.get(), &var);
      VariantClear(&var);
    }
  }

  // Put caret back to start (before ghost).
  ComPtr<ITfRange> start;
  if (SUCCEEDED(range->Clone(start.put())) && start.get()) {
    start->Collapse(ecWrite, TF_ANCHOR_START);
    TF_SELECTION ns = {};
    ns.range = start.get();
    ns.style.ase = TF_AE_NONE;
    ns.style.fInterimChar = FALSE;
    pic->SetSelection(ecWrite, 1, &ns);
  }

  return S_OK;
}

HRESULT TextService::AcceptGhostInEditSession(ITfContext* pic, TfEditCookie ecWrite) {
  IgnoreTextEditsScope ignore(ignore_text_edits_depth_);
  if (!pic || !ghost_composition_) {
    return S_OK;
  }

  ComPtr<ITfRange> range;
  if (FAILED(ghost_composition_->GetRange(range.put())) || !range.get()) {
    ghost_composition_->Release();
    ghost_composition_ = nullptr;
    return S_OK;
  }

  // Clear display attribute on committed text.
  ComPtr<ITfProperty> prop;
  if (SUCCEEDED(pic->GetProperty(GUID_PROP_ATTRIBUTE, prop.put())) && prop.get()) {
    prop->Clear(ecWrite, range.get());
  }

  // Move caret to end after committing.
  ComPtr<ITfRange> end;
  if (SUCCEEDED(range->Clone(end.put())) && end.get()) {
    end->Collapse(ecWrite, TF_ANCHOR_END);

    ghost_composition_->EndComposition(ecWrite);
    ghost_composition_->Release();
    ghost_composition_ = nullptr;

    TF_SELECTION ns = {};
    ns.range = end.get();
    ns.style.ase = TF_AE_NONE;
    ns.style.fInterimChar = FALSE;
    pic->SetSelection(ecWrite, 1, &ns);
  } else {
    ghost_composition_->EndComposition(ecWrite);
    ghost_composition_->Release();
    ghost_composition_ = nullptr;
  }

  return S_OK;
}
