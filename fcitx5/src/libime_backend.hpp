#pragma once

#include <memory>
#include <string>
#include <vector>

namespace aetherime {

class LibImeBackend {
public:
    LibImeBackend();
    ~LibImeBackend();

    bool available() const { return available_; }
    const std::string &status() const { return status_; }

    std::vector<std::string> query(const std::string &pinyin, size_t limit) const;

private:
    bool available_ = false;
    std::string status_ = "libime backend not initialized";

#ifdef AETHERIME_HAS_LIBIME
    struct Impl;
    std::unique_ptr<Impl> impl_;
#endif
};

} // namespace aetherime
