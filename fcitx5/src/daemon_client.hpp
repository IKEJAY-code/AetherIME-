#pragma once

#include <optional>
#include <string>
#include <vector>

namespace aetherime {

enum class Language {
    Zh,
    En,
};

enum class PredictMode {
    Next,
    Fim,
};

struct PredictionRequest {
    std::string prefix;
    std::string suffix;
    Language language = Language::Zh;
    PredictMode mode = PredictMode::Fim;
    int maxTokens = 12;
    int latencyBudgetMs = 90;
};

struct PredictionResult {
    std::string ghostText;
    std::vector<std::string> candidates;
    float confidence = 0.0f;
    std::string source;
    int elapsedMs = 0;
};

class DaemonClient {
public:
    explicit DaemonClient(std::string socketPath);

    bool ping() const;
    std::optional<PredictionResult> predict(const PredictionRequest &request) const;

private:
    std::optional<std::string> request(const std::string &payload) const;

    std::string socketPath_;
};

} // namespace aetherime
