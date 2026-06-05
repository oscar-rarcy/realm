#include "render/sdl/sdl_profiler.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

namespace {

struct SectionStats {
    long long samples = 0;
    double totalMs = 0.0;
    double maxMs = 0.0;
};

bool gEnabled = false;
long long gFrames = 0;
std::map<std::string, SectionStats> gSections;

double profilerNowMs() {
    using Clock = std::chrono::steady_clock;
    using MsDouble = std::chrono::duration<double, std::milli>;
    return std::chrono::duration_cast<MsDouble>(Clock::now().time_since_epoch()).count();
}

std::string jsonEscape(const std::string& text) {
    std::ostringstream out;
    for (char ch : text) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << ch; break;
        }
    }
    return out.str();
}

} // namespace

bool realmProfilerEnabled() {
    return gEnabled;
}

void realmProfilerSetEnabled(bool enabled) {
    gEnabled = enabled;
}

void realmProfilerReset() {
    gFrames = 0;
    gSections.clear();
}

void realmProfilerBeginFrame() {
    if (!gEnabled) return;
}

void realmProfilerEndFrame() {
    if (!gEnabled) return;
    ++gFrames;
}

void realmProfilerAddSample(const char* section, double ms) {
    if (!gEnabled || !section) return;
    SectionStats& stats = gSections[section];
    stats.samples += 1;
    stats.totalMs += ms;
    stats.maxMs = std::max(stats.maxMs, ms);
}

RealmProfileScope::RealmProfileScope(const char* section)
    : section_(section), startMs_(0.0), active_(realmProfilerEnabled()) {
    if (active_) startMs_ = profilerNowMs();
}

RealmProfileScope::~RealmProfileScope() {
    if (!active_) return;
    realmProfilerAddSample(section_, profilerNowMs() - startMs_);
}

void realmProfilerWriteReports(const std::string& jsonPath, const std::string& csvPath) {
    if (!gEnabled) return;

    std::vector<std::pair<std::string, SectionStats>> sections(gSections.begin(), gSections.end());
    std::sort(sections.begin(), sections.end(), [](const auto& a, const auto& b) {
        return a.second.totalMs > b.second.totalMs;
    });

    if (!jsonPath.empty()) {
        std::filesystem::create_directories(std::filesystem::path(jsonPath).parent_path());
        std::ofstream out(jsonPath, std::ios::binary);
        out << std::fixed << std::setprecision(4);
        out << "{\n";
        out << "  \"schema\": \"realm.sdl_profile.v1\",\n";
        out << "  \"frames\": " << gFrames << ",\n";
        out << "  \"sections\": [\n";
        for (size_t i = 0; i < sections.size(); ++i) {
            const auto& name = sections[i].first;
            const auto& stats = sections[i].second;
            double avg = stats.samples > 0 ? stats.totalMs / (double)stats.samples : 0.0;
            double perFrame = gFrames > 0 ? stats.totalMs / (double)gFrames : 0.0;
            out << "    {\"name\": \"" << jsonEscape(name) << "\", "
                << "\"samples\": " << stats.samples << ", "
                << "\"total_ms\": " << stats.totalMs << ", "
                << "\"avg_ms\": " << avg << ", "
                << "\"max_ms\": " << stats.maxMs << ", "
                << "\"per_frame_ms\": " << perFrame << "}";
            if (i + 1 < sections.size()) out << ",";
            out << "\n";
        }
        out << "  ]\n";
        out << "}\n";
    }

    if (!csvPath.empty()) {
        std::filesystem::create_directories(std::filesystem::path(csvPath).parent_path());
        std::ofstream out(csvPath, std::ios::binary);
        out << "section,samples,total_ms,avg_ms,max_ms,per_frame_ms\n";
        out << std::fixed << std::setprecision(4);
        for (const auto& [name, stats] : sections) {
            double avg = stats.samples > 0 ? stats.totalMs / (double)stats.samples : 0.0;
            double perFrame = gFrames > 0 ? stats.totalMs / (double)gFrames : 0.0;
            out << '"' << name << '"' << ','
                << stats.samples << ','
                << stats.totalMs << ','
                << avg << ','
                << stats.maxMs << ','
                << perFrame << "\n";
        }
    }

    std::cerr << "realm: profiler frames=" << gFrames << "\n";
    for (const auto& [name, stats] : sections) {
        double perFrame = gFrames > 0 ? stats.totalMs / (double)gFrames : 0.0;
        std::cerr << "realm: profile " << name << " per_frame_ms=" << perFrame
                  << " total_ms=" << stats.totalMs << " samples=" << stats.samples << "\n";
    }
}
