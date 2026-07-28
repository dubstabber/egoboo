#pragma once

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>

#include "egolib/Graphics/IFont.hpp"
#include "egolib/Graphics/VertexFormat.hpp"
#include "egolib/game/GUI/IUIManager.hpp"

namespace Ego::Test {

/// @brief A no-op renderer for laid out text produced by @a HeadlessFont. Never touches GL.
class HeadlessLaidTextRenderer : public Ego::ILaidTextRenderer {
public:
    void render(int, int, const Ego::Colour4f & = Ego::Colour4f::white()) override {}
};

/// @brief A deterministic, GL/SDL_ttf-free stand-in for Ego::Font. Metrics are computed from
///        the text alone (8 pixels per character wide, 16 pixels per line tall) so widget
///        layout arithmetic is exercised and reproducible without a real font atlas.
class HeadlessFont : public Ego::IFont {
public:
    static constexpr int GLYPH_WIDTH_PX = 8;
    static constexpr int LINE_HEIGHT_PX = 16;

    void getTextSize(const std::string &text, int *width, int *height) override {
        if (width) *width = GLYPH_WIDTH_PX * static_cast<int>(text.size());
        if (height) *height = LINE_HEIGHT_PX;
    }

    void drawTextToTexture(Ego::Texture *, const std::string &, const Ego::Colour3f & = Ego::Colour3f::white()) override {}

    void drawText(const std::string &, int, int, const Ego::Colour4f & = Ego::Colour4f::white()) override {}

    void drawTextBox(const std::string &, int, int, int, int, int, const Ego::Colour4f & = Ego::Colour4f::white()) override {}

    std::shared_ptr<Ego::ILaidTextRenderer> layoutText(const std::string &text, int *textWidth, int *textHeight) override {
        getTextSize(text, textWidth, textHeight);
        return std::make_shared<HeadlessLaidTextRenderer>();
    }

    std::shared_ptr<Ego::ILaidTextRenderer> layoutTextBox(const std::string &text, int /*width*/, int /*height*/, int /*spacing*/,
                                                          int *textWidth, int *textHeight) override {
        int longestLine = 0;
        int lineCount = 0;
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) {
            longestLine = std::max<int>(longestLine, static_cast<int>(line.size()));
            ++lineCount;
        }
        if (text.empty()) {
            lineCount = 0;
        }
        if (textWidth) *textWidth = GLYPH_WIDTH_PX * longestLine;
        if (textHeight) *textHeight = LINE_HEIGHT_PX * std::max(1, lineCount);
        return std::make_shared<HeadlessLaidTextRenderer>();
    }

    int getLineSpacing() const override { return LINE_HEIGHT_PX; }
};

/// @brief A properly-constructed, GL/SDL_ttf-free Ego::GUI::IUIManager for headless tests.
///        Serves one shared @a HeadlessFont for every font slot and no-ops every drawing
///        method, so GUI widgets (Label, Button, ModuleSelector, ...) can construct and lay
///        out text through the same @a Ego::GUI::activeUIManager() seam the running engine
///        uses, without a live renderer or video buffer manager.
class HeadlessUIManager : public Ego::GUI::IUIManager {
public:
    explicit HeadlessUIManager(int screenWidth = 640, int screenHeight = 480) :
        _screenWidth(screenWidth),
        _screenHeight(screenHeight),
        _font(std::make_shared<HeadlessFont>()),
        _vertexDescriptor(Ego::descriptor_factory<idlib::vertex_format::P2F>()()) {
    }

    std::shared_ptr<Ego::IFont> getFont(const UIFontType) const override { return _font; }

    int getScreenWidth() const override { return _screenWidth; }
    int getScreenHeight() const override { return _screenHeight; }

    void beginRenderUI() override {}
    void endRenderUI() override {}

    void drawImage(const Ego::Point2f &, const Ego::Vector2f &, const std::shared_ptr<const Ego::GUI::Material> &) override {}

    bool dumpScreenshot() override { return false; }

    float drawBitmapFontString(const Ego::Vector2f &start, const std::string &, const uint32_t = 0, const float = 1.0f) override {
        return start.y() + HeadlessFont::LINE_HEIGHT_PX;
    }

    void fillRectangle(const Ego::Rectangle2f &, const bool, const Ego::Colour4f & = Ego::Colour4f::white()) override {}

    void drawQuad2D(const Ego::Rectangle2f &, const Ego::Rectangle2f &, const std::shared_ptr<const Ego::GUI::Material> &) override {}
    void drawQuad2D(const Ego::Rectangle2f &, const ego_frect_t &, const std::shared_ptr<const Ego::GUI::Material> &) override {}

    void drawQuad2d(const Ego::Rectangle2f &, const Ego::Rectangle2f &) override {}
    void drawQuad2d(const Ego::Rectangle2f &) override {}

    const idlib::vertex_descriptor& componentVertexDescriptor() const override { return _vertexDescriptor; }
    const std::shared_ptr<idlib::vertex_buffer>& componentVertexBuffer() const override {
        static const std::shared_ptr<idlib::vertex_buffer> nullBuffer;
        return nullBuffer;
    }

private:
    int _screenWidth;
    int _screenHeight;
    std::shared_ptr<HeadlessFont> _font;
    idlib::vertex_descriptor _vertexDescriptor;
};

/// @brief RAII installer for a @a HeadlessUIManager (or any @a IUIManager) into the GUI-layer
///        active-UIManager seam, restoring whatever was previously installed (if anything) on
///        destruction. Keeps headless-suite test ordering independent: a test that installs a
///        manager never leaks it into a subsequent test that asserts none is installed.
class ScopedActiveUIManager {
public:
    explicit ScopedActiveUIManager(Ego::GUI::IUIManager& uiManager) :
        _previous(Ego::GUI::tryActiveUIManager()) {
        Ego::GUI::installActiveUIManager(uiManager);
    }

    ~ScopedActiveUIManager() {
        if (_previous) {
            Ego::GUI::installActiveUIManager(*_previous);
        } else {
            Ego::GUI::clearActiveUIManager();
        }
    }

    ScopedActiveUIManager(const ScopedActiveUIManager&) = delete;
    ScopedActiveUIManager& operator=(const ScopedActiveUIManager&) = delete;

private:
    Ego::GUI::IUIManager* _previous;
};

} // namespace Ego::Test
