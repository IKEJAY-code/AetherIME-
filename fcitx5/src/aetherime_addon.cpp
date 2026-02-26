#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fcitx-utils/inputbuffer.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>

#include "ghost_session.hpp"
#include "libime_backend.hpp"

namespace aetherime {

class AetherImeEngine;

namespace {

const std::array<fcitx::Key, 10> kSelectionKeys = {
    fcitx::Key{FcitxKey_1}, fcitx::Key{FcitxKey_2}, fcitx::Key{FcitxKey_3},
    fcitx::Key{FcitxKey_4}, fcitx::Key{FcitxKey_5}, fcitx::Key{FcitxKey_6},
    fcitx::Key{FcitxKey_7}, fcitx::Key{FcitxKey_8}, fcitx::Key{FcitxKey_9},
    fcitx::Key{FcitxKey_0},
};

fcitx::KeyList makeSelectionKeyList() {
    fcitx::KeyList list;
    list.reserve(kSelectionKeys.size());
    for (const auto &key : kSelectionKeys) {
        list.emplace_back(key);
    }
    return list;
}

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

void appendUnique(std::vector<std::string> &output, const std::vector<std::string> &input,
                  size_t limit) {
    for (const auto &entry : input) {
        if (entry.empty()) {
            continue;
        }
        if (std::find(output.begin(), output.end(), entry) != output.end()) {
            continue;
        }
        output.push_back(entry);
        if (output.size() >= limit) {
            return;
        }
    }
}

const std::unordered_map<std::string, std::vector<std::string>> &fallbackZhLexicon() {
    static const auto lexicon = std::unordered_map<std::string, std::vector<std::string>>{
        {"ni", {"你", "呢", "泥"}},
        {"nihao", {"你好", "你好吗", "你好呀"}},
        {"wo", {"我", "握", "窝"}},
        {"women", {"我们", "我们先", "我们可以"}},
        {"jintian", {"今天", "今天的", "今天我们"}},
        {"xiexie", {"谢谢", "谢谢你", "谢谢大家"}},
        {"qingwen", {"请问", "请问一下", "请问现在方便吗"}},
        {"woxiang", {"我想", "我想要", "我想先"}},
        {"ceshi", {"测试", "测试一下", "测试完成"}},
    };
    return lexicon;
}

const std::unordered_map<std::string, std::vector<std::string>> &enLexicon() {
    static const auto lexicon = std::unordered_map<std::string, std::vector<std::string>>{
        {"hello", {"hello", "hello there", "hello team"}},
        {"please", {"please", "please review", "please help"}},
        {"thanks", {"thanks", "thanks a lot", "thanks for your help"}},
        {"build", {"build", "build this", "build the feature"}},
        {"need", {"need", "need to", "need your help"}},
    };
    return lexicon;
}

} // namespace

class AetherImeState;

class AetherImeCandidateWord : public fcitx::CandidateWord {
public:
    AetherImeCandidateWord(AetherImeState *state, std::string text);

    void select(fcitx::InputContext *inputContext) const override;

private:
    AetherImeState *state_;
    std::string text_;
};

class AetherImeEngine final : public fcitx::InputMethodEngineV2 {
public:
    explicit AetherImeEngine(fcitx::Instance *instance);

    void keyEvent(const fcitx::InputMethodEntry &entry, fcitx::KeyEvent &keyEvent) override;
    void reset(const fcitx::InputMethodEntry &entry, fcitx::InputContextEvent &event) override;

    std::string subModeLabelImpl(const fcitx::InputMethodEntry &entry,
                                 fcitx::InputContext &ic) override;
    std::string subModeIconImpl(const fcitx::InputMethodEntry &entry,
                                fcitx::InputContext &ic) override;

    auto factory() const { return &factory_; }
    auto instance() const { return instance_; }
    const std::string &socketPath() const { return socketPath_; }
    const LibImeBackend &libimeBackend() const { return *libimeBackend_; }

private:
    fcitx::Instance *instance_;
    std::string socketPath_;
    std::shared_ptr<LibImeBackend> libimeBackend_;
    fcitx::FactoryFor<AetherImeState> factory_;
};

class AetherImeState final : public fcitx::InputContextProperty {
public:
    AetherImeState(AetherImeEngine *engine, fcitx::InputContext *ic)
        : engine_(engine),
          ic_(ic),
          ghostSession_(DaemonClient(engine->socketPath())),
          buffer_({fcitx::InputBufferOption::AsciiOnly, fcitx::InputBufferOption::FixedCursor}) {}

    void keyEvent(fcitx::KeyEvent &event);
    void reset();
    void onEngineReset();
    void commitCandidateText(const std::string &text);

    bool englishMode() const { return englishMode_; }

private:
    void toggleEnglishMode();
    void togglePredict();
    void updatePrediction(const std::string &contextTail = {});
    void updateUI();
    std::vector<std::string> lexicalCandidates() const;
    std::pair<std::string, std::string> buildPredictContext(const std::string &predictBase) const;
    void commitAndRefresh(const std::string &text);

    AetherImeEngine *engine_;
    fcitx::InputContext *ic_;
    GhostSession ghostSession_;
    fcitx::InputBuffer buffer_;
    bool englishMode_ = false;
    bool predictEnabled_ = true;
    std::string ghostText_;
    std::string predictionSource_;
    std::vector<std::string> mergedCandidates_;
};

AetherImeCandidateWord::AetherImeCandidateWord(AetherImeState *state, std::string text)
    : state_(state), text_(std::move(text)) {
    setText(fcitx::Text(text_));
}

void AetherImeCandidateWord::select(fcitx::InputContext *inputContext) const {
    FCITX_UNUSED(inputContext);
    state_->commitCandidateText(text_);
}

AetherImeEngine::AetherImeEngine(fcitx::Instance *instance)
    : instance_(instance),
      socketPath_([] {
          const char *socket = std::getenv("AETHERIME_SOCKET");
          if (socket && *socket) {
              return std::string(socket);
          }
          return std::string("/tmp/aetherime.sock");
      }()),
      libimeBackend_(std::make_shared<LibImeBackend>()),
      factory_([this](fcitx::InputContext &ic) { return new AetherImeState(this, &ic); }) {
    instance_->inputContextManager().registerProperty("aetherimeState", &factory_);
    FCITX_INFO() << "AetherIME pinyin backend status: " << libimeBackend_->status();
}

void AetherImeEngine::keyEvent(const fcitx::InputMethodEntry &entry, fcitx::KeyEvent &keyEvent) {
    FCITX_UNUSED(entry);
    if (keyEvent.isRelease()) {
        return;
    }
    auto *state = keyEvent.inputContext()->propertyFor(&factory_);
    state->keyEvent(keyEvent);
}

void AetherImeEngine::reset(const fcitx::InputMethodEntry &entry, fcitx::InputContextEvent &event) {
    FCITX_UNUSED(entry);
    auto *state = event.inputContext()->propertyFor(&factory_);
    state->onEngineReset();
}

std::string AetherImeEngine::subModeLabelImpl(const fcitx::InputMethodEntry &entry,
                                            fcitx::InputContext &ic) {
    FCITX_UNUSED(entry);
    auto *state = ic.propertyFor(&factory_);
    if (!state) {
        return "中";
    }
    return state->englishMode() ? "EN" : "中";
}

std::string AetherImeEngine::subModeIconImpl(const fcitx::InputMethodEntry &entry,
                                           fcitx::InputContext &ic) {
    FCITX_UNUSED(entry);
    FCITX_UNUSED(ic);
    return "input-keyboard";
}

void AetherImeState::keyEvent(fcitx::KeyEvent &event) {
    if (event.key().check(FcitxKey_semicolon, fcitx::KeyState::Ctrl)) {
        togglePredict();
        event.filterAndAccept();
        return;
    }

    if (event.key().check(FcitxKey_space, fcitx::KeyState::Ctrl)) {
        toggleEnglishMode();
        event.filterAndAccept();
        return;
    }

    if (auto candidateList = ic_->inputPanel().candidateList()) {
        const int selectionIndex = event.key().keyListIndex(kSelectionKeys);
        if (selectionIndex >= 0 && selectionIndex < candidateList->size()) {
            candidateList->candidate(selectionIndex).select(ic_);
            event.filterAndAccept();
            return;
        }
        if (event.key().check(FcitxKey_Up)) {
            if (auto *cursorMovable = candidateList->toCursorMovable()) {
                cursorMovable->prevCandidate();
                ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
                event.filterAndAccept();
            }
            return;
        }
        if (event.key().check(FcitxKey_Down)) {
            if (auto *cursorMovable = candidateList->toCursorMovable()) {
                cursorMovable->nextCandidate();
                ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
                event.filterAndAccept();
            }
            return;
        }
        if (event.key().checkKeyList(engine_->instance()->globalConfig().defaultPrevPage())) {
            if (auto *pageable = candidateList->toPageable(); pageable && pageable->hasPrev()) {
                pageable->prev();
                ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
            }
            event.filterAndAccept();
            return;
        }
        if (event.key().checkKeyList(engine_->instance()->globalConfig().defaultNextPage())) {
            if (auto *pageable = candidateList->toPageable(); pageable && pageable->hasNext()) {
                pageable->next();
                ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
            }
            event.filterAndAccept();
            return;
        }
    }

    if (event.key().check(FcitxKey_Tab)) {
        if (!ghostText_.empty()) {
            if (buffer_.empty()) {
                commitAndRefresh(ghostText_);
            } else {
                const std::string base = mergedCandidates_.empty() ? buffer_.userInput()
                                                                    : mergedCandidates_.front();
                commitAndRefresh(base + ghostText_);
            }
            event.filterAndAccept();
            return;
        }
        if (!buffer_.empty()) {
            commitAndRefresh(buffer_.userInput());
            event.filterAndAccept();
        }
        return;
    }

    if (event.key().check(FcitxKey_Escape)) {
        if (!buffer_.empty() || !ghostText_.empty()) {
            reset();
            event.filterAndAccept();
        }
        return;
    }

    if (event.key().check(FcitxKey_BackSpace)) {
        if (!buffer_.empty() && buffer_.backspace()) {
            updatePrediction();
            updateUI();
            event.filterAndAccept();
        }
        return;
    }

    if (event.key().check(FcitxKey_Return)) {
        if (!buffer_.empty()) {
            commitAndRefresh(buffer_.userInput());
            event.filterAndAccept();
        }
        return;
    }

    if (event.key().check(FcitxKey_space)) {
        if (!buffer_.empty() && !mergedCandidates_.empty()) {
            commitCandidateText(mergedCandidates_.front());
            event.filterAndAccept();
            return;
        }
        if (!buffer_.empty()) {
            commitAndRefresh(buffer_.userInput());
            event.filterAndAccept();
            return;
        }
        return;
    }

    if (englishMode_ && buffer_.empty()) {
        return;
    }

    auto normalized = event.key().normalize();
    if (!normalized.isSimple()) {
        if (!buffer_.empty()) {
            event.filterAndAccept();
        }
        return;
    }

    if (buffer_.type(normalized.sym())) {
        updatePrediction();
        updateUI();
        event.filterAndAccept();
    }
}

void AetherImeState::reset() {
    buffer_.clear();
    ghostSession_.clearGhost();
    ghostText_.clear();
    predictionSource_.clear();
    mergedCandidates_.clear();
    updateUI();
}

void AetherImeState::onEngineReset() {
    buffer_.clear();
    mergedCandidates_.clear();
    updateUI();
}

void AetherImeState::commitCandidateText(const std::string &text) {
    commitAndRefresh(text);
}

void AetherImeState::commitAndRefresh(const std::string &text) {
    if (text.empty()) {
        return;
    }
    ic_->commitString(text);
    buffer_.clear();
    mergedCandidates_.clear();
    predictionSource_.clear();
    ghostText_.clear();
    ghostSession_.clearGhost();
    updatePrediction(text);
    updateUI();
}

void AetherImeState::toggleEnglishMode() {
    englishMode_ = !englishMode_;
    reset();
}

void AetherImeState::togglePredict() {
    predictEnabled_ = !predictEnabled_;
    updatePrediction();
    updateUI();
}

std::vector<std::string> AetherImeState::lexicalCandidates() const {
    const auto code = toLowerAscii(buffer_.userInput());
    if (code.empty()) {
        return {};
    }

    if (!englishMode_) {
        const auto libimeCandidates = engine_->libimeBackend().query(code, 5);
        if (!libimeCandidates.empty()) {
            return libimeCandidates;
        }
    }

    const auto &table = englishMode_ ? enLexicon() : fallbackZhLexicon();
    auto iterator = table.find(code);
    if (iterator == table.end()) {
        return {};
    }
    return iterator->second;
}

std::pair<std::string, std::string> AetherImeState::buildPredictContext(
    const std::string &predictBase) const {
    std::string prefix = predictBase;
    std::string suffix;

    const auto &surrounding = ic_->surroundingText();
    if (!surrounding.isValid()) {
        return {prefix, suffix};
    }
    const auto &text = surrounding.text();
    if (!fcitx::utf8::validate(text)) {
        return {prefix, suffix};
    }
    if (text.empty()) {
        return {prefix, suffix};
    }

    const auto totalChars = fcitx::utf8::length(text);
    const auto cursorChars = std::min<size_t>(surrounding.cursor(), totalChars);
    const auto beforeChars = std::min<size_t>(256, cursorChars);
    const auto afterChars = std::min<size_t>(128, totalChars - cursorChars);

    const auto startByte = fcitx::utf8::ncharByteLength(text.begin(), cursorChars - beforeChars);
    const auto cursorByte = fcitx::utf8::ncharByteLength(text.begin(), cursorChars);
    const auto endByte = fcitx::utf8::ncharByteLength(text.begin(), cursorChars + afterChars);

    prefix = text.substr(startByte, cursorByte - startByte) + prefix;
    suffix = text.substr(cursorByte, endByte - cursorByte);
    return {prefix, suffix};
}

void AetherImeState::updatePrediction(const std::string &contextTail) {
    mergedCandidates_.clear();
    predictionSource_.clear();
    ghostText_.clear();

    if (!buffer_.empty()) {
        const auto lexical = lexicalCandidates();
        appendUnique(mergedCandidates_, lexical, 5);
        return;
    }

    if (!predictEnabled_) {
        return;
    }

    auto [prefix, suffix] = buildPredictContext(contextTail);
    if (prefix.empty() && suffix.empty()) {
        return;
    }

    ghostSession_.setLanguage(englishMode_ ? Language::En : Language::Zh);
    ghostSession_.setMode(PredictMode::Fim);
    ghostText_ = ghostSession_.onTextChanged(prefix, suffix);

    if (const auto &prediction = ghostSession_.lastPrediction(); prediction) {
        predictionSource_ = prediction->source;
    }
}

void AetherImeState::updateUI() {
    auto &inputPanel = ic_->inputPanel();
    const bool active = !buffer_.empty() || !ghostText_.empty() || !mergedCandidates_.empty();
    inputPanel.reset();
    if (!active) {
        if (ic_->capabilityFlags().test(fcitx::CapabilityFlag::Preedit)) {
            inputPanel.setClientPreedit(fcitx::Text());
        } else {
            inputPanel.setPreedit(fcitx::Text());
        }
        ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
        ic_->updatePreedit();
        return;
    }

    if (!mergedCandidates_.empty()) {
        auto candidateList = std::make_unique<fcitx::CommonCandidateList>();
        candidateList->setSelectionKey(makeSelectionKeyList());
        candidateList->setPageSize(5);
        for (const auto &candidate : mergedCandidates_) {
            candidateList->append<AetherImeCandidateWord>(this, candidate);
        }
        inputPanel.setCandidateList(std::move(candidateList));
    }

    fcitx::Text preedit;
    if (!buffer_.empty()) {
        preedit.append(buffer_.userInput(), fcitx::TextFormatFlag::HighLight);
    }
    if (!ghostText_.empty()) {
        fcitx::TextFormatFlags ghostFlags;
        ghostFlags |= fcitx::TextFormatFlag::Italic;
        preedit.append(ghostText_, ghostFlags);
    }

    if (ic_->capabilityFlags().test(fcitx::CapabilityFlag::Preedit)) {
        inputPanel.setClientPreedit(preedit);
    } else {
        inputPanel.setPreedit(preedit);
    }

    inputPanel.setAuxUp(fcitx::Text(englishMode_ ? "EN" : "中"));
    std::string status = predictEnabled_ ? "AI:on" : "AI:off";
    if (!predictionSource_.empty()) {
        status += " " + predictionSource_;
    }
    if (engine_->libimeBackend().available()) {
        status += " PY:libime";
    } else if (!englishMode_) {
        status += " PY:fallback";
    }
    inputPanel.setAuxDown(fcitx::Text(status));

    ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    ic_->updatePreedit();
}

class AetherImeFactory final : public fcitx::AddonFactory {
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        return new AetherImeEngine(manager->instance());
    }
};

} // namespace aetherime

FCITX_ADDON_FACTORY(aetherime::AetherImeFactory)
