#include "Log.h"

#include <cstdarg>
#include <cstdio>
#include <string>

namespace {

std::wstring FormatV(const wchar_t* fmt, va_list ap) {
  wchar_t buf[1024];
  buf[0] = L'\0';
  _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
  return std::wstring(buf);
}

void AppendToTempFile(const std::wstring& line) {
  wchar_t tmpPath[MAX_PATH];
  DWORD n = GetTempPathW(_countof(tmpPath), tmpPath);
  if (n == 0 || n >= _countof(tmpPath)) {
    return;
  }
  std::wstring path = std::wstring(tmpPath) + L"shurufa_tsf.log";

  HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    return;
  }

  std::wstring withNewline = line;
  withNewline.append(L"\r\n");
  DWORD bytes = 0;
  WriteFile(h, withNewline.data(), static_cast<DWORD>(withNewline.size() * sizeof(wchar_t)), &bytes, nullptr);
  CloseHandle(h);
}

void Emit(const wchar_t* level, const std::wstring& msg) {
  std::wstring line = std::wstring(L"[shurufa][") + level + L"] " + msg;
  OutputDebugStringW(line.c_str());
  OutputDebugStringW(L"\r\n");
  AppendToTempFile(line);
}

} // namespace

namespace shurufa::log {

void Info(const wchar_t* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  auto msg = FormatV(fmt, ap);
  va_end(ap);
  Emit(L"info", msg);
}

void Error(const wchar_t* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  auto msg = FormatV(fmt, ap);
  va_end(ap);
  Emit(L"error", msg);
}

} // namespace shurufa::log

