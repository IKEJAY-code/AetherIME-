#include "ghost_session.hpp"

namespace aetherime {

GhostSession::GhostSession(DaemonClient client) : client_(std::move(client)) {}

void GhostSession::setLanguage(Language language) { language_ = language; }

void GhostSession::setMode(PredictMode mode) { mode_ = mode; }

std::string GhostSession::onTextChanged(const std::string &prefix, const std::string &suffix) {
    PredictionRequest request{
        .prefix = prefix,
        .suffix = suffix,
        .language = language_,
        .mode = mode_,
        .maxTokens = 8,
        .latencyBudgetMs = 5000,
    };

    lastPrediction_ = client_.predict(request);
    if (!lastPrediction_ || lastPrediction_->ghostText.empty()) {
        lastPrediction_.reset();
        ghostText_.clear();
        return ghostText_;
    }
    ghostText_ = lastPrediction_->ghostText;
    return ghostText_;
}

std::string GhostSession::acceptGhost() {
    std::string accepted = ghostText_;
    ghostText_.clear();
    if (lastPrediction_) {
        lastPrediction_->ghostText.clear();
    }
    return accepted;
}

void GhostSession::clearGhost() {
    ghostText_.clear();
    lastPrediction_.reset();
}

} // namespace aetherime
