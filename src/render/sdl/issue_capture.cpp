#include "render/sdl/sdl_capture.h"
#include "realm.h"
#include "core/game_events.h"
#include "view_state.h"

std::string captureTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y%m%d-%H%M%S")
       << '-' << std::setw(3) << std::setfill('0') << ms.count();
    return ss.str();
}

void captureIssueBundle() {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path dir = fs::absolute(fs::path("captures") / ("realm-issue-" + captureTimestamp()), ec);
    if (ec) dir = fs::path("captures") / ("realm-issue-" + captureTimestamp());
    fs::create_directories(dir, ec);
    if (ec) {
        emitUiStatusEvent(-1, "Issue capture failed.");
        std::cerr << "realm: issue capture mkdir failed " << dir.string()
                  << " error=" << ec.message() << "\n";
        return;
    }

    fs::path savePath = dir / "realm-save.txt";
    fs::path shotPath = dir / "screenshot.bmp";
    fs::path infoPath = dir / "capture-info.txt";

    bool saved = saveGame(g, savePath.string());
    bool shot = gfxSaveScreenshot(shotPath.string());

    std::ofstream info(infoPath);
    if (info) {
        info << "Realm issue capture\n"
             << "directory: " << dir.string() << "\n"
             << "save: " << savePath.filename().string() << "\n"
             << "screenshot: " << shotPath.filename().string() << "\n"
             << "tick: " << g.tick << "\n"
             << "seed: " << g.seed << "\n"
             << "cursor: " << view.cursorX << "," << view.cursorY << "\n"
             << "view: " << view.viewX << "," << view.viewY << " "
             << view.viewW << "x" << view.viewH << "\n"
             << "projection: " << (displayMode == DM_EMOJI ? "isometric" : "grid") << "\n"
             << "visuals: " << (displayMode == DM_EMOJI ? "tileset" : "ascii") << "\n"
             << "window: " << s.winW << "x" << s.winH << "\n";
    }

    int clip = SDL_SetClipboardText(dir.string().c_str());
    if (saved && shot && clip == 0) {
        emitUiStatusEvent(-1, "Issue capture saved; path copied.");
    } else if (saved || shot) {
        emitUiStatusEvent(-1, "Issue capture partial; see log.");
    } else {
        emitUiStatusEvent(-1, "Issue capture failed.");
    }

    std::cerr << "realm: issue capture dir=" << dir.string()
              << " save=" << (saved ? "ok" : "failed")
              << " screenshot=" << (shot ? "ok" : "failed")
              << " clipboard=" << (clip == 0 ? "ok" : SDL_GetError()) << "\n";
}
