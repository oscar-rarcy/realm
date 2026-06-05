#pragma once

#include <string>

bool realmProfilerEnabled();
void realmProfilerSetEnabled(bool enabled);
void realmProfilerReset();
void realmProfilerBeginFrame();
void realmProfilerEndFrame();
void realmProfilerAddSample(const char* section, double ms);
void realmProfilerWriteReports(const std::string& jsonPath, const std::string& csvPath);

class RealmProfileScope {
public:
    explicit RealmProfileScope(const char* section);
    ~RealmProfileScope();

    RealmProfileScope(const RealmProfileScope&) = delete;
    RealmProfileScope& operator=(const RealmProfileScope&) = delete;

private:
    const char* section_;
    double startMs_;
    bool active_;
};
