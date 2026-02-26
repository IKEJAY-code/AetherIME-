#include "libime_backend.hpp"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#ifdef AETHERIME_HAS_LIBIME
#include <libime/core/userlanguagemodel.h>
#include <libime/pinyin/pinyincontext.h>
#include <libime/pinyin/pinyindictionary.h>
#include <libime/pinyin/pinyinime.h>
#endif

namespace aetherime {

namespace {

#ifdef AETHERIME_HAS_LIBIME
std::string envOrEmpty(const char *name) {
    const char *value = std::getenv(name);
    if (!value || !*value) {
        return {};
    }
    return value;
}

std::string firstExistingPath(const std::vector<std::string> &candidates) {
    for (const auto &candidate : candidates) {
        if (candidate.empty()) {
            continue;
        }
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

bool isLikelyPinyinInput(const std::string &text) {
    if (text.empty()) {
        return false;
    }
    return std::all_of(text.begin(), text.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '\'';
    });
}
#else
bool isLikelyPinyinInput(const std::string &text) {
    if (text.empty()) {
        return false;
    }
    return std::all_of(text.begin(), text.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '\'';
    });
}
#endif

} // namespace

#ifdef AETHERIME_HAS_LIBIME
struct LibImeBackend::Impl {
    std::unique_ptr<libime::PinyinIME> ime;
};
#endif

LibImeBackend::LibImeBackend() {
#ifdef AETHERIME_HAS_LIBIME
    try {
        auto dictPath = envOrEmpty("AETHERIME_LIBIME_DICT");
        if (dictPath.empty()) {
            dictPath =
                firstExistingPath({"/usr/share/libime/sc.dict", "/usr/local/share/libime/sc.dict"});
        }
        auto modelPath = envOrEmpty("AETHERIME_LIBIME_LM");
        if (modelPath.empty()) {
            modelPath =
                firstExistingPath({"/usr/lib/x86_64-linux-gnu/libime/zh_CN.lm",
                                   "/usr/lib/aarch64-linux-gnu/libime/zh_CN.lm",
                                   "/usr/lib64/libime/zh_CN.lm", "/usr/lib/libime/zh_CN.lm",
                                   "/usr/local/lib/libime/zh_CN.lm"});
        }

        if (dictPath.empty()) {
            status_ = "libime dict file not found (expect sc.dict)";
            return;
        }
        if (modelPath.empty()) {
            status_ = "libime language model file not found (expect zh_CN.lm)";
            return;
        }

        auto dict = std::make_unique<libime::PinyinDictionary>();
        dict->load(libime::PinyinDictionary::SystemDict, dictPath.c_str(),
                   libime::PinyinDictFormat::Binary);

        auto model = std::make_unique<libime::UserLanguageModel>(modelPath.c_str());
        auto ime = std::make_unique<libime::PinyinIME>(std::move(dict), std::move(model));
        ime->setBeamSize(20);
        ime->setNBest(2);
        ime->setScoreFilter(1.0F);

        impl_ = std::make_unique<Impl>();
        impl_->ime = std::move(ime);
        available_ = true;
        status_ = "libime ready";
    } catch (const std::exception &error) {
        status_ = std::string("libime init failed: ") + error.what();
        available_ = false;
    }
#else
    status_ = "built without libime";
    available_ = false;
#endif
}

LibImeBackend::~LibImeBackend() = default;

std::vector<std::string> LibImeBackend::query(const std::string &pinyin, size_t limit) const {
    if (!available_ || pinyin.empty() || limit == 0 || !isLikelyPinyinInput(pinyin)) {
        return {};
    }

#ifdef AETHERIME_HAS_LIBIME
    std::set<std::string> dedup;
    std::vector<std::string> output;
    output.reserve(limit);

    try {
        libime::PinyinContext context(impl_->ime.get());
        context.type(pinyin);

        const auto &candidates = context.candidatesToCursor().empty() ? context.candidates()
                                                                       : context.candidatesToCursor();
        for (const auto &candidate : candidates) {
            auto text = candidate.toString();
            if (text.empty()) {
                continue;
            }
            if (!dedup.insert(text).second) {
                continue;
            }
            output.push_back(std::move(text));
            if (output.size() >= limit) {
                break;
            }
        }
    } catch (...) {
        return {};
    }
    return output;
#else
    (void)pinyin;
    (void)limit;
    return {};
#endif
}

} // namespace aetherime
