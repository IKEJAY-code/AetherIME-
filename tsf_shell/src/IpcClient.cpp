#include "IpcClient.h"

#include "Log.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <chrono>
#include <cstdlib>
#include <string>
#include <utility>

#pragma comment(lib, "ws2_32.lib")

namespace {

std::string Utf8FromWide(const std::wstring& w) {
  if (w.empty()) {
    return {};
  }
  int needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
  if (needed <= 0) {
    return {};
  }
  std::string out;
  out.resize(static_cast<size_t>(needed));
  WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), needed, nullptr, nullptr);
  return out;
}

std::wstring WideFromUtf8(const std::string& s) {
  if (s.empty()) {
    return {};
  }
  int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  if (needed <= 0) {
    return {};
  }
  std::wstring out;
  out.resize(static_cast<size_t>(needed));
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), needed);
  return out;
}

std::string JsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char ch : s) {
    switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out.push_back(ch);
        break;
    }
  }
  return out;
}

bool SendAll(SOCKET s, const std::string& data) {
  const char* p = data.data();
  int left = static_cast<int>(data.size());
  while (left > 0) {
    int n = send(s, p, left, 0);
    if (n == SOCKET_ERROR) {
      return false;
    }
    p += n;
    left -= n;
  }
  return true;
}

// Extremely small JSON extractor (Phase 1): parses known fields from EngineMessage::Suggestion.
bool ExtractJsonStringField(const std::string& json, const char* key, std::string* out) {
  const std::string needle = std::string("\"") + key + "\":\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  pos += needle.size();

  std::string s;
  s.reserve(64);
  bool esc = false;
  for (; pos < json.size(); ++pos) {
    char ch = json[pos];
    if (esc) {
      switch (ch) {
        case 'n':
          s.push_back('\n');
          break;
        case 'r':
          s.push_back('\r');
          break;
        case 't':
          s.push_back('\t');
          break;
        case '\\':
        case '"':
          s.push_back(ch);
          break;
        default:
          s.push_back(ch);
          break;
      }
      esc = false;
      continue;
    }
    if (ch == '\\') {
      esc = true;
      continue;
    }
    if (ch == '"') {
      *out = std::move(s);
      return true;
    }
    s.push_back(ch);
  }
  return false;
}

bool ExtractJsonFloatField(const std::string& json, const char* key, float* out) {
  const std::string needle = std::string("\"") + key + "\":";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  pos += needle.size();
  while (pos < json.size() && (json[pos] == ' ')) {
    ++pos;
  }
  char* end = nullptr;
  float v = static_cast<float>(strtod(json.c_str() + pos, &end));
  if (end == json.c_str() + pos) {
    return false;
  }
  *out = v;
  return true;
}

bool ExtractJsonU32Array2Field(const std::string& json, const char* key, uint32_t* a, uint32_t* b) {
  const std::string needle = std::string("\"") + key + "\":[";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  pos += needle.size();
  char* end = nullptr;
  unsigned long v1 = strtoul(json.c_str() + pos, &end, 10);
  if (end == json.c_str() + pos) {
    return false;
  }
  pos = static_cast<size_t>(end - json.c_str());
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == ',')) {
    ++pos;
  }
  unsigned long v2 = strtoul(json.c_str() + pos, &end, 10);
  if (end == json.c_str() + pos) {
    return false;
  }
  *a = static_cast<uint32_t>(v1);
  *b = static_cast<uint32_t>(v2);
  return true;
}

bool ParseSuggestionResponseLine(const std::string& line, SuggestionResponse* out) {
  if (line.find("\"type\":\"suggestion\"") == std::string::npos) {
    return false;
  }

  std::string requestId;
  std::string suggestion;
  float confidence = 0.0f;
  uint32_t rs = 0, re = 0;

  if (!ExtractJsonStringField(line, "request_id", &requestId)) {
    return false;
  }
  // suggestion can be empty; treat missing as empty too.
  ExtractJsonStringField(line, "suggestion", &suggestion);
  ExtractJsonFloatField(line, "confidence", &confidence);
  ExtractJsonU32Array2Field(line, "replace_range", &rs, &re);

  out->request_id = WideFromUtf8(requestId);
  out->suggestion = WideFromUtf8(suggestion);
  out->confidence = confidence;
  out->replace_start_utf16 = rs;
  out->replace_end_utf16 = re;
  return true;
}

SOCKET ConnectTcp(const std::string& host, uint16_t port) {
  addrinfo hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrinfo* result = nullptr;
  char portStr[16];
  _snprintf_s(portStr, _countof(portStr), _TRUNCATE, "%u", port);
  if (getaddrinfo(host.c_str(), portStr, &hints, &result) != 0) {
    return INVALID_SOCKET;
  }

  SOCKET sock = INVALID_SOCKET;
  for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock == INVALID_SOCKET) {
      continue;
    }
    if (connect(sock, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) == 0) {
      break;
    }
    closesocket(sock);
    sock = INVALID_SOCKET;
  }
  freeaddrinfo(result);

  if (sock != INVALID_SOCKET) {
    BOOL nodelay = TRUE;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
  }
  return sock;
}

} // namespace

IpcClient::IpcClient() = default;

IpcClient::~IpcClient() { Stop(); }

void IpcClient::SetEndpoint(const std::string& host, uint16_t port) {
  std::lock_guard<std::mutex> lock(mu_);
  host_ = host;
  port_ = port;
}

void IpcClient::Start(Callback cb) {
  Stop();
  cb_ = std::move(cb);
  stop_ = false;
  worker_ = std::thread([this] { ThreadMain(); });
}

void IpcClient::Stop() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    stop_ = true;
  }
  if (worker_.joinable()) {
    worker_.join();
  }
  {
    std::lock_guard<std::mutex> lock(mu_);
    outbox_.clear();
  }
}

void IpcClient::SendSuggest(const SuggestRequest& req) {
  std::string requestId = Utf8FromWide(req.request_id);
  std::string context = Utf8FromWide(req.context);
  std::string hint = Utf8FromWide(req.language_hint.empty() ? L"auto" : req.language_hint);

  std::string json;
  json.reserve(context.size() + 256);
  json += "{\"type\":\"suggest\",\"request_id\":\"";
  json += JsonEscape(requestId);
  json += "\",\"context\":\"";
  json += JsonEscape(context);
  json += "\",\"cursor\":";
  json += std::to_string(req.cursor_utf16);
  json += ",\"language_hint\":\"";
  json += JsonEscape(hint);
  json += "\",\"max_len\":";
  json += std::to_string(req.max_len);
  json += "}\n";

  std::lock_guard<std::mutex> lock(mu_);
  outbox_.push_back(std::move(json));
}

void IpcClient::SendCancel(const std::wstring& request_id) {
  std::string requestId = Utf8FromWide(request_id);
  std::string json;
  json.reserve(128);
  json += "{\"type\":\"cancel\",\"request_id\":\"";
  json += JsonEscape(requestId);
  json += "\"}\n";
  std::lock_guard<std::mutex> lock(mu_);
  outbox_.push_back(std::move(json));
}

void IpcClient::ThreadMain() {
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    shurufa::log::Error(L"WSAStartup failed");
    return;
  }

  SOCKET sock = INVALID_SOCKET;
  std::string recvBuf;
  recvBuf.reserve(4096);

  for (;;) {
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (stop_) {
        break;
      }
    }

    if (sock == INVALID_SOCKET) {
      std::string host;
      uint16_t port = 0;
      {
        std::lock_guard<std::mutex> lock(mu_);
        host = host_;
        port = port_;
      }

      sock = ConnectTcp(host, port);
      if (sock == INVALID_SOCKET) {
        // Keep stop() responsive (avoid long sleeps during deactivate).
        for (int i = 0; i < 6; ++i) {
          {
            std::lock_guard<std::mutex> lock(mu_);
            if (stop_) {
              break;
            }
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        continue;
      }
      shurufa::log::Info(L"IpcClient connected %S:%u", host.c_str(), port);
    }

    // Send queued messages (best-effort).
    for (;;) {
      std::string msg;
      {
        std::lock_guard<std::mutex> lock(mu_);
        if (outbox_.empty()) {
          break;
        }
        msg = std::move(outbox_.front());
        outbox_.pop_front();
      }
      if (!SendAll(sock, msg)) {
        shurufa::log::Error(L"IpcClient send failed; reconnect");
        closesocket(sock);
        sock = INVALID_SOCKET;
        break;
      }
    }

    if (sock == INVALID_SOCKET) {
      continue;
    }

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);
    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 20 * 1000; // 20ms

    int r = select(0, &rfds, nullptr, nullptr, &tv);
    if (r == SOCKET_ERROR) {
      shurufa::log::Error(L"IpcClient select error; reconnect");
      closesocket(sock);
      sock = INVALID_SOCKET;
      continue;
    }

    if (r > 0 && FD_ISSET(sock, &rfds)) {
      char buf[2048];
      int n = recv(sock, buf, sizeof(buf), 0);
      if (n <= 0) {
        shurufa::log::Error(L"IpcClient recv closed; reconnect");
        closesocket(sock);
        sock = INVALID_SOCKET;
        continue;
      }
      recvBuf.append(buf, buf + n);

      for (;;) {
        size_t nl = recvBuf.find('\n');
        if (nl == std::string::npos) {
          break;
        }
        std::string line = recvBuf.substr(0, nl);
        recvBuf.erase(0, nl + 1);

        SuggestionResponse rsp;
        if (ParseSuggestionResponseLine(line, &rsp)) {
          if (cb_) {
            cb_(rsp);
          }
        }
      }
    }
  }

  if (sock != INVALID_SOCKET) {
    closesocket(sock);
  }
  WSACleanup();
}
