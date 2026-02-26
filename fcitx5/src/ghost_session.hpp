#pragma once

#include <optional>
#include <string>

#include "daemon_client.hpp"

namespace aetherime {

class GhostSession {
public:
    explicit GhostSession(DaemonClient client);

    void setLanguage(Language language);
    void setMode(PredictMode mode);

    std::string onTextChanged(const std::string &prefix, const std::string &suffix);
    std::string acceptGhost();
    void clearGhost();

    const std::optional<PredictionResult> &lastPrediction() const { return lastPrediction_; }
    const std::string &ghost() const { return ghostText_; }

private:
    DaemonClient client_;
    Language language_ = Language::Zh;
    PredictMode mode_ = PredictMode::Fim;
    std::optional<PredictionResult> lastPrediction_;
    std::string ghostText_;
};

} // namespace aetherime
