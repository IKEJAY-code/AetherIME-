#include "daemon_client.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <regex>
#include <sstream>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace aetherime {
namespace {

std::string escapeJson(const std::string &value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
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
            out += c;
            break;
        }
    }
    return out;
}

std::string unescapeJson(const std::string &value) {
    std::string out;
    out.reserve(value.size());
    bool escaped = false;
    for (char c : value) {
        if (!escaped && c == '\\') {
            escaped = true;
            continue;
        }
        if (escaped) {
            switch (c) {
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 't':
                out.push_back('\t');
                break;
            default:
                out.push_back(c);
                break;
            }
            escaped = false;
            continue;
        }
        out.push_back(c);
    }
    return out;
}

std::optional<std::string> extractStringField(const std::string &payload, const std::string &field) {
    std::regex pattern("\"" + field + "\"\\s*:\\s*\"((?:\\\\.|[^\"])*)\"");
    std::smatch match;
    if (!std::regex_search(payload, match, pattern) || match.size() < 2) {
        return std::nullopt;
    }
    return unescapeJson(match[1].str());
}

std::vector<std::string> extractStringArray(const std::string &payload, const std::string &field) {
    std::vector<std::string> result;
    std::regex fieldPattern("\"" + field + "\"\\s*:\\s*\\[(.*?)\\]");
    std::smatch fieldMatch;
    if (!std::regex_search(payload, fieldMatch, fieldPattern) || fieldMatch.size() < 2) {
        return result;
    }

    std::string arrayBody = fieldMatch[1].str();
    std::regex itemPattern("\"((?:\\\\.|[^\"])*)\"");
    for (std::sregex_iterator iterator(arrayBody.begin(), arrayBody.end(), itemPattern);
         iterator != std::sregex_iterator(); ++iterator) {
        result.push_back(unescapeJson((*iterator)[1].str()));
    }
    return result;
}

std::optional<float> extractFloatField(const std::string &payload, const std::string &field) {
    std::regex pattern("\"" + field + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
    std::smatch match;
    if (!std::regex_search(payload, match, pattern) || match.size() < 2) {
        return std::nullopt;
    }
    return std::stof(match[1].str());
}

std::optional<int> extractIntField(const std::string &payload, const std::string &field) {
    std::regex pattern("\"" + field + "\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    if (!std::regex_search(payload, match, pattern) || match.size() < 2) {
        return std::nullopt;
    }
    return std::stoi(match[1].str());
}

} // namespace

DaemonClient::DaemonClient(std::string socketPath) : socketPath_(std::move(socketPath)) {}

bool DaemonClient::ping() const {
    auto response = request(R"({"id":"ping","type":"ping"})");
    return response.has_value() && response->find("\"type\":\"pong\"") != std::string::npos;
}

std::optional<PredictionResult> DaemonClient::predict(const PredictionRequest &requestValue) const {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream payload;
    payload << "{\"id\":\"" << now << "\",\"type\":\"predict\",\"prefix\":\""
            << escapeJson(requestValue.prefix) << "\",\"suffix\":\""
            << escapeJson(requestValue.suffix) << "\",\"language\":\""
            << (requestValue.language == Language::Zh ? "zh" : "en") << "\",\"mode\":\""
            << (requestValue.mode == PredictMode::Fim ? "fim" : "next")
            << "\",\"max_tokens\":" << requestValue.maxTokens
            << ",\"latency_budget_ms\":" << requestValue.latencyBudgetMs << "}";

    auto response = request(payload.str());
    if (!response || response->find("\"type\":\"error\"") != std::string::npos) {
        return std::nullopt;
    }
    if (response->find("\"type\":\"predict\"") == std::string::npos) {
        return std::nullopt;
    }

    PredictionResult result;
    result.ghostText = extractStringField(*response, "ghost_text").value_or("");
    result.candidates = extractStringArray(*response, "candidates");
    result.confidence = extractFloatField(*response, "confidence").value_or(0.0f);
    result.source = extractStringField(*response, "source").value_or("");
    result.elapsedMs = extractIntField(*response, "elapsed_ms").value_or(0);
    return result;
}

std::optional<std::string> DaemonClient::request(const std::string &payload) const {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return std::nullopt;
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (socketPath_.size() >= sizeof(address.sun_path)) {
        close(fd);
        return std::nullopt;
    }
    std::strncpy(address.sun_path, socketPath_.c_str(), sizeof(address.sun_path) - 1);

    if (connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        close(fd);
        return std::nullopt;
    }

    std::string framedPayload = payload + "\n";
    ssize_t written = send(fd, framedPayload.data(), framedPayload.size(), 0);
    if (written < 0) {
        close(fd);
        return std::nullopt;
    }

    std::string response;
    std::array<char, 1024> buffer{};
    while (true) {
        ssize_t readBytes = recv(fd, buffer.data(), buffer.size(), 0);
        if (readBytes <= 0) {
            break;
        }
        response.append(buffer.data(), static_cast<size_t>(readBytes));
        if (response.find('\n') != std::string::npos) {
            break;
        }
    }

    close(fd);

    auto newline = response.find('\n');
    if (newline != std::string::npos) {
        response = response.substr(0, newline);
    }
    if (response.empty()) {
        return std::nullopt;
    }
    return response;
}

} // namespace aetherime
