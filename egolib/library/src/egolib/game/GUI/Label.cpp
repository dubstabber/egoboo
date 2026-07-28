#include "egolib/Graphics/Font.hpp"
#include "egolib/game/GUI/Label.hpp"

namespace Ego {
namespace GUI {

Label::Label() : Label(std::string()) {
}

Label::Label(const std::string &text, const UIManager::UIFontType font) :
    _text(text),
    _font(),
    _colour(Colour4f::white()),
    _textRenderer(),
    _fontType(font),
    _textLayoutPending(false) {
    // No UI manager yet (headless construction): leave _font null, fetched lazily by setText().
    if (auto *ui = tryActiveUIManager()) {
        _font = ui->getFont(font);
    }
    if (!text.empty()) {
        setText(text);
    }
}

void Label::draw(DrawingContext& drawingContext) {
    // Self-heal a text layout deferred by a headless construction/setText() call.
    if (_textLayoutPending) {
        setText(_text);
    }
    // Draw text.
    if (_textRenderer)
        _textRenderer->render(getDerivedPosition().x(), getDerivedPosition().y(), _colour);
}

const std::string& Label::getText() const {
    return _text;
}

void Label::setText(const std::string& text) {
    _text = text;

    // Fetch the font lazily if a UI manager has appeared since construction.
    auto *ui = tryActiveUIManager();
    if (!_font && ui) {
        _font = ui->getFont(_fontType);
    }
    if (!_font && !ui) {
        // Still headless: defer layout until draw() self-heals it.
        _textRenderer = nullptr;
        _textLayoutPending = true;
        return;
    }

    // Recalculate our size.
    int textWidth, textHeight;
    _textRenderer = _font->layoutTextBox(_text, 0, 0, _font->getLineSpacing(), &textWidth, &textHeight);
    setSize(Vector2f(textWidth, textHeight));
    _textLayoutPending = false;
}

const std::shared_ptr<Font>& Label::getFont() const {
    return _font;
}

void Label::setFont(const std::shared_ptr<Font>& font) {
    _font = font;

    // Recalculate our size.
    int textWidth, textHeight;
    _textRenderer = _font->layoutTextBox(_text, 0, 0, _font->getLineSpacing(), &textWidth, &textHeight);
    setSize(Vector2f(textWidth, textHeight));
    _textLayoutPending = false;
}

const Colour4f& Label::getColour() const {
    return _colour;
}

void Label::setColour(const Colour4f& colour) {
    _colour = colour;
}

void Label::setAlpha(float a) {
    _colour.set_alpha(a);
}

float Label::getAlpha() const {
    return _colour.get_alpha();
}

} // namespace GUI
} // namespace Ego