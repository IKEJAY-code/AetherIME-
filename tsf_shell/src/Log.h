#pragma once

#include <windows.h>

namespace shurufa::log {

void Info(const wchar_t* fmt, ...);
void Error(const wchar_t* fmt, ...);

} // namespace shurufa::log

