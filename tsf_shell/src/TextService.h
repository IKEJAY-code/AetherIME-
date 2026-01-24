#pragma once

#include "IpcClient.h"

#include <msctf.h>
#include <windows.h>

#include <atomic>
#include <string>

class ClearGhostEditSession;
class ShowGhostEditSession;
class AcceptGhostEditSession;

class TextService final : public ITfTextInputProcessor,
                          public ITfKeyEventSink,
                          public ITfThreadMgrEventSink,
                          public ITfTextEditSink,
                          public ITfCompositionSink,
                          public ITfDisplayAttributeProvider {
 public:
  TextService();

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // ITfTextInputProcessor
  STDMETHODIMP Activate(ITfThreadMgr* ptim, TfClientId tid) override;
  STDMETHODIMP Deactivate() override;

  // ITfKeyEventSink
  STDMETHODIMP OnSetFocus(BOOL fForeground) override;
  STDMETHODIMP OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
  STDMETHODIMP OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
  STDMETHODIMP OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
  STDMETHODIMP OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
  STDMETHODIMP OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten) override;

  // ITfThreadMgrEventSink
  STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* /*pDocMgr*/) override { return S_OK; }
  STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* /*pDocMgr*/) override { return S_OK; }
  STDMETHODIMP OnSetFocus(ITfDocumentMgr* pDocMgrFocus, ITfDocumentMgr* pDocMgrPrevFocus) override;
  STDMETHODIMP OnPushContext(ITfContext* /*pContext*/) override { return S_OK; }
  STDMETHODIMP OnPopContext(ITfContext* /*pContext*/) override { return S_OK; }

  // ITfTextEditSink
  STDMETHODIMP OnEndEdit(ITfContext* pic, TfEditCookie ecReadOnly, ITfEditRecord* pEditRecord) override;

  // ITfCompositionSink
  STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition* pComposition) override;

  // ITfDisplayAttributeProvider
  STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** ppEnum) override;
  STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** ppInfo) override;

 private:
  ~TextService();
  friend class ClearGhostEditSession;
  friend class ShowGhostEditSession;
  friend class AcceptGhostEditSession;

  struct SuggestionPayload {
    std::wstring request_id;
    std::wstring suggestion;
    float confidence = 0.0f;
    uint32_t replace_start_utf16 = 0;
    uint32_t replace_end_utf16 = 0;
  };

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  HRESULT CreateMessageWindow();
  void DestroyMessageWindow();

  void ScheduleDebounce();
  void CancelDebounce();
  void SendPendingRequest();

  void PostSuggestionToTsfThread(const SuggestionResponse& rsp);
  void HandleSuggestionOnTsfThread(SuggestionPayload* payload);

  bool IsSensitiveInput(ITfContext* pic, TfEditCookie ecReadOnly);
  bool ReadContextBeforeCursor(ITfContext* pic, TfEditCookie ecReadOnly, std::wstring* out_before,
                               uint32_t max_chars);

  bool HasGhost() const { return ghost_composition_ != nullptr; }

  void RequestClearGhost(ITfContext* pic);
  void RequestShowGhost(ITfContext* pic, const std::wstring& suggestion);
  void RequestAcceptGhost(ITfContext* pic);

  HRESULT ClearGhostInEditSession(ITfContext* pic, TfEditCookie ecWrite);
  HRESULT ShowGhostInEditSession(ITfContext* pic, TfEditCookie ecWrite, const std::wstring& suggestion);
  HRESULT AcceptGhostInEditSession(ITfContext* pic, TfEditCookie ecWrite);

  LONG refCount_ = 1;

  ITfThreadMgr* thread_mgr_ = nullptr;
  TfClientId client_id_ = TF_CLIENTID_NULL;

  DWORD cookie_threadmgr_sink_ = TF_INVALID_COOKIE;
  DWORD cookie_textedit_sink_ = TF_INVALID_COOKIE;

  ITfContext* focus_context_ = nullptr;

  ITfComposition* ghost_composition_ = nullptr;
  TfGuidAtom ghost_display_attr_atom_ = TF_INVALID_GUIDATOM;

  HWND msg_hwnd_ = nullptr;
  UINT_PTR debounce_timer_id_ = 0;

  std::wstring pending_context_;
  uint32_t pending_cursor_utf16_ = 0;

  std::atomic<uint64_t> next_request_id_{1};
  std::wstring inflight_request_id_;

  std::atomic<long> ignore_text_edits_depth_{0};

  IpcClient ipc_;
};
