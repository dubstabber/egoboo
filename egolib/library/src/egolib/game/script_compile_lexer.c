//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/// @file egolib/game/script_compile_lexer.c
/// @brief line_scanner_state_t method bodies (the EgoScript lexical scanner). Split off
///        script_compile.c on 2026-06-12 (~358 lines, all line_scanner_state_t:: methods).
/// @details The struct is declared in the public script_compile.h header, so no header
///          carving was needed. parser_state_t token parsing, opcode emission, and syntax
///          diagnostics now live in script_compile_parser.c.

#include "egolib/game/script_compile.h"
#include "egolib/Core/StringUtilities.hpp"
#include "egolib/Script/CLogEntry.hpp"
#include "egolib/Log/_Include.hpp"

line_scanner_state_t::line_scanner_state_t(Ego::Script::Buffer *inputBuffer, const idlib::hll::location& location)
    : m_token(Ego::Script::PDLTokenKind::StartOfLine, location, location), m_inputPosition(0),
      m_inputBuffer(inputBuffer), m_location(location), m_lexemeBuffer()
{}

idlib::hll::location line_scanner_state_t::getLocation() const
{
    return m_location;
}

void line_scanner_state_t::next()
{
    if (m_inputPosition < m_inputBuffer->getSize()) m_inputPosition++;
}

void line_scanner_state_t::write(int symbol)
{
    m_lexemeBuffer.append(symbol);
}

void line_scanner_state_t::save()
{
    write(getCurrent());
}

void line_scanner_state_t::saveAndNext()
{
    save();
    next();
}

void line_scanner_state_t::writeAndNext(int symbol)
{
    write(symbol);
    next();
}

int line_scanner_state_t::getCurrent() const
{
    if (m_inputPosition >= m_inputBuffer->getSize()) return EndOfInputSymbol();
    else return m_inputBuffer->get(m_inputPosition);
}

bool line_scanner_state_t::is(int symbol) const
{
    return symbol == getCurrent();
}

bool line_scanner_state_t::isOneOf(int symbol1, int symbol2) const
{
    return is(symbol1)
        || is(symbol2);
}

bool line_scanner_state_t::isDoubleQuote() const
{
    return is(DoubleQuoteSymbol());
}

bool line_scanner_state_t::isStartOfInput() const
{
    return is(StartOfInputSymbol());
}

bool line_scanner_state_t::isEndOfInput() const
{
    return is(EndOfInputSymbol());
}

bool line_scanner_state_t::isWhiteSpace() const
{
	auto x = getCurrent();
	if (x < std::numeric_limits<char>::min() || x > std::numeric_limits<char>::max())
	{
		return false;
	}
	return Ego::isspace((char)x);
}

bool line_scanner_state_t::isDigit() const
{
	auto x = getCurrent();
	if (x < std::numeric_limits<char>::min() || x > std::numeric_limits<char>::max())
	{
		return false;
	}
	return Ego::isdigit((char)x);
}

bool line_scanner_state_t::isAlphabetic() const
{
	auto x = getCurrent();
	if (x < std::numeric_limits<char>::min() || x > std::numeric_limits<char>::max())
	{
		return false;
	}
	return Ego::isalpha((char)x);
}

bool line_scanner_state_t::isNewLine() const
{
    return is(LinefeedSymbol())
        || is(CarriageReturnSymbol());
}

bool line_scanner_state_t::isOperator() const
{
    return isOneOf('+', '-',
                   '*', '/',
                   '%',
                   '>', '<',
                   '&', '=');
}

bool line_scanner_state_t::isControl() const
{
	auto x = getCurrent();
	if (x < std::numeric_limits<char>::min() || x > std::numeric_limits<char>::max())
	{
		return false;
	}
    return Ego::iscntrl((char)x);
}

void line_scanner_state_t::emit(const Ego::Script::PDLToken& token)
{
    m_token = token;
}

void line_scanner_state_t::emit(Ego::Script::PDLTokenKind kind, const idlib::hll::location& start, const idlib::hll::location& end)
{
    m_token = Ego::Script::PDLToken(kind, start, end);
}

void line_scanner_state_t::emit(Ego::Script::PDLTokenKind kind, const idlib::hll::location& start, const idlib::hll::location& end,
                                const std::string& lexeme)
{
    m_token = Ego::Script::PDLToken(kind, start, end, lexeme);
}

void line_scanner_state_t::emit(Ego::Script::PDLTokenKind kind, const idlib::hll::location& start, const idlib::hll::location& end,
                                int value)
{
    m_token = Ego::Script::PDLToken(kind, start, end);
    m_token.setValue(value);
}

Ego::Script::PDLToken line_scanner_state_t::scanWhiteSpaces()
{
    auto startLocation = getLocation();
    int numberOfWhiteSpaces = 0;
    if (isWhiteSpace())
    {
        do
        {
            if (is(TabulatorSymbol()))
            {
                Ego::Script::CLogEntry e(Log::Level::Warning, __FILE__, __LINE__, __FUNCTION__, getLocation());
                e << "tabulator character in source file" << Log::EndOfEntry;
                Log::activeTarget() << e;
            }
            numberOfWhiteSpaces++;
            next();
        } while (isWhiteSpace());
    }
    auto endLocation = getLocation();
    emit(Ego::Script::PDLTokenKind::Whitespace, startLocation, endLocation, numberOfWhiteSpaces);
    return m_token;
}

Ego::Script::PDLToken line_scanner_state_t::scanNewLines()
{
    auto startLocation = getLocation();
    int numberOfNewLines = 0;
    while (isNewLine())
    {
        // '\n' | '\r' | '\n\r' | '\r\n'
        int old = getCurrent();
        next();
        if (old != getCurrent() && isNewLine())
        {
            next();
        }
        m_location = idlib::hll::location(m_location.file_name(), m_location.line_number() + 1);
        numberOfNewLines++;
    }
    auto endLocation = getLocation();
    emit(Ego::Script::PDLTokenKind::Newline, startLocation, endLocation, numberOfNewLines);
    return m_token;
}

Ego::Script::PDLToken line_scanner_state_t::scanNumericLiteral()
{
    m_lexemeBuffer.clear();
    auto startLocation = getLocation();
    if (!isDigit())
    {
        throw idlib::runtime_error(__FILE__, __LINE__, "<internal error>");
    }
    do
    {
        saveAndNext();
    } while (isDigit());
    auto endLocation = getLocation();
    emit(Ego::Script::PDLTokenKind::NumberLiteral, startLocation,
         endLocation, m_lexemeBuffer.toString());
    return m_token;
}

Ego::Script::PDLToken line_scanner_state_t::scanName()
{
    m_lexemeBuffer.clear();
    auto startLocation = getLocation();
    if (!is('_') && !isAlphabetic())
    {
        throw idlib::runtime_error(__FILE__, __LINE__, "<internal error>");
    }
    do
    {
        saveAndNext();
    } while (is('_') || isDigit() || isAlphabetic());
    auto endLocation = getLocation();
    emit(Ego::Script::PDLTokenKind::Name, startLocation, endLocation,
         m_lexemeBuffer.toString());
    return m_token;
}

Ego::Script::PDLToken line_scanner_state_t::scanStringOrReference()
{
    m_lexemeBuffer.clear();
    auto startLocation = getLocation();
    if (!isDoubleQuote())
    {
        throw idlib::runtime_error(__FILE__, __LINE__, "<internal error>");
    }

    next(); // Skip leading quotation mark.
    // If a string starts with a shebang, its contents is considered as a reference.
    bool isReference = is('#');
    if (isReference) next();


    while (!isEndOfInput() && !isNewLine() && !isDoubleQuote())
    {
        if (is(TabulatorSymbol()))
        {
            // convert tab characters to the '~' symbol.
            writeAndNext(TildeSymbol());
        }
        else if (isWhiteSpace() || isControl())
        {
            // whitespace symbols and control symbols are converted to the '_' symbol.
            writeAndNext(UnderscoreSymbol());
        }
        else
        {
            saveAndNext();
        }
    }

    if (isDoubleQuote())
    {
        next(); // Skip ending quotation mark.
    }
    else /* if (isNewline() || isEndOfInput()) */
    {
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, getLocation(), "unclosed string literal");
    }
    auto endLocation = getLocation();
    emit(isReference ? Ego::Script::PDLTokenKind::ReferenceLiteral : Ego::Script::PDLTokenKind::StringLiteral,
         startLocation, endLocation, m_lexemeBuffer.toString());
    return m_token;
}

Ego::Script::PDLToken line_scanner_state_t::scanIDSZ()
{
    m_lexemeBuffer.clear();
    auto startLocation = getLocation();
    if (!is('['))
    {
        throw idlib::runtime_error(__FILE__, __LINE__, "<internal error>");
    }
    saveAndNext();
    for (auto i = 0; i < 4; ++i)
    {
        if (!isDigit() && !isAlphabetic())
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, getLocation(), "invalid IDSZ");
        }
        saveAndNext();
    }
    if (!is(']'))
    {
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, getLocation(), "invalid IDSZ");
    }
    saveAndNext();
    auto endLocation = getLocation();
    emit(Ego::Script::PDLTokenKind::IdszLiteral, startLocation,
         endLocation, m_lexemeBuffer.toString());
    return m_token;
}

Ego::Script::PDLToken line_scanner_state_t::scanOperator()
{
    m_lexemeBuffer.clear();
    auto startLocation = getLocation();
    if (!isOperator())
    {
        throw idlib::runtime_error(__FILE__, __LINE__, "<internal error>");
    }
    auto current = getCurrent();
    saveAndNext();
    auto endLocation = getLocation();
    switch (current)
    {
        case '+':
            emit(Ego::Script::PDLTokenKind::Plus, startLocation, endLocation,
                 m_lexemeBuffer.toString());
            break;
        case '-':
            emit(Ego::Script::PDLTokenKind::Minus, startLocation, endLocation,
                 m_lexemeBuffer.toString());
            break;
        case '*':
            emit(Ego::Script::PDLTokenKind::Multiply, startLocation, endLocation,
                 m_lexemeBuffer.toString());
            break;
        case '/':
            emit(Ego::Script::PDLTokenKind::Divide, startLocation, endLocation,
                 m_lexemeBuffer.toString());
            break;
        case '%':
            emit(Ego::Script::PDLTokenKind::Modulus, startLocation, endLocation,
                 m_lexemeBuffer.toString());
            break;
        case '>':
            emit(Ego::Script::PDLTokenKind::ShiftRight, startLocation, endLocation,
                 m_lexemeBuffer.toString());
            break;
        case '<':
            emit(Ego::Script::PDLTokenKind::ShiftLeft, startLocation, endLocation,
                 m_lexemeBuffer.toString());
            break;
        case '&':
            emit(Ego::Script::PDLTokenKind::And, startLocation, endLocation,
                 m_lexemeBuffer.toString());
            break;
        case '=':
            emit(Ego::Script::PDLTokenKind::Assign, startLocation, endLocation,
                 m_lexemeBuffer.toString());
            break;
        default:
            throw idlib::runtime_error(__FILE__, __LINE__, "internal error");
    }
    return m_token;
}
