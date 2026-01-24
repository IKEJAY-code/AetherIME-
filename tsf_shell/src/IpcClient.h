#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <deque>

struct SuggestRequest {
  std::wstring request_id;
  std::wstring context;
  uint32_t cursor_utf16 = 0;
  uint32_t max_len = 32;
  std::wstring language_hint; // "auto" | "en" | "zh" | "mixed"
};

struct SuggestionResponse {
  std::wstring request_id;
  std::wstring suggestion;
  float confidence = 0.0f;
  uint32_t replace_start_utf16 = 0;
  uint32_t replace_end_utf16 = 0;
};

class IpcClient final {
 public:
  using Callback = std::function<void(const SuggestionResponse&)>;

  IpcClient();
  ~IpcClient();

  IpcClient(const IpcClient&) = delete;
  IpcClient& operator=(const IpcClient&) = delete;

  void SetEndpoint(const std::string& host, uint16_t port);

  void Start(Callback cb);
  void Stop();

  void SendSuggest(const SuggestRequest& req);
  void SendCancel(const std::wstring& request_id);

 private:
  void ThreadMain();

  std::string host_ = "127.0.0.1";
  uint16_t port_ = 48080;

  Callback cb_;

  std::mutex mu_;
  std::deque<std::string> outbox_;
  bool stop_ = false;

  std::thread worker_;
};

