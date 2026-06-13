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

/// @file egolib/ReadContext_literals.cpp
/// @brief Implementation of ReadContext DDL literal-parser member functions

#include "egolib/fileutil.h"

#include "egolib/Script/DDLTokenDecoder.hpp"
#include "egolib/Log/_Include.hpp"
#include "egolib/strutil.h"
#include "egolib/platform.h"
#include "egolib/_math.h"

#pragma push_macro("ERROR")
#undef ERROR

Ego::Script::DDLToken ReadContext::parseStringLiteral()
{
    idlib::hll::location startLocation = get_location();
    clear_lexeme_text();
    while (true)
    {
        if (ise(ERROR()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "read error");
        }
        else if (ise(TILDE()))
        {
            write_and_next('\t');
        }
        else if (ise(UNDERSCORE()))
        {
            write_and_next(' ');
        }
        else if (ise(NEW_LINE()) || ise(WHITE_SPACE()) || ise(END_OF_INPUT()))
        {
            break;
        }
        else
        {
            save_and_next();
        }
    }
    return Ego::Script::DDLToken(Ego::Script::DDLTokenKind::String, startLocation, get_lexeme_text());
}

Ego::Script::DDLToken ReadContext::parseCharacterLiteral() {
    idlib::hll::location startLocation = get_location();
    clear_lexeme_text();
    if (ise(END_OF_INPUT()) || ise(ERROR()))
    {
        if (ise(ERROR()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "read error while scanning character literal");
        }
        else if (ise(END_OF_INPUT()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "premature end of input while scanning character literal");
        }
    }
    if (!ise(SINGLE_QUOTE()))
    {
        if (ise(ERROR()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "read error while scanning character literal");
        }
        else if (ise(END_OF_INPUT()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "premature end of input while scanning character literal");
        }
        else
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "unexpected character while scanning character literal");
        }
    }
    next();
    if (ise(BACKSLASH()))
    {
        next();
        if (ise(SINGLE_QUOTE()))
        {
            write_and_next('\'');
        }
        else if (is('n'))
        {
            write_and_next('\n');
        }
        else if (is('t'))
        {
            write_and_next('\t');
        }
        else if (ise(BACKSLASH()))
        {
            write_and_next('\\');
        }
        else
        {
            if (ise(ERROR()))
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "read error while scanning character literal");
            }
            else if (ise(END_OF_INPUT()))
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "premature end of input while scanning character literal");
            }
            else
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "unknown/unsupported escape sequence");
            }
        }
    }
    else
    {
        if (ise(END_OF_INPUT()) || ise(ERROR()))
        {
            if (ise(ERROR()))
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "read error while scanning character literal");
            }
            else if (ise(END_OF_INPUT()))
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "empty character literal");
            }
        }
        save_and_next();
    }
    if (!ise(SINGLE_QUOTE())) {
        throw Ego::Script::MissingDelimiterError(__FILE__, __LINE__, get_location(), '\'');
    }
    next();
    return Ego::Script::DDLToken(Ego::Script::DDLTokenKind::Character, startLocation, get_lexeme_text());
}

Ego::Script::DDLToken ReadContext::parseIntegerLiteral()
{
    idlib::hll::location startLocation = get_location();
    clear_lexeme_text();
    if (ise(PLUS()) || ise(MINUS()))
    {
        save_and_next();
    }
    if (!ise(DIGIT()))
    {
        if (ise(ERROR()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "read error while scanning integer literal");
        }
        else if (ise(END_OF_INPUT()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "premature end of input while scanning integer literal");
        }
        else
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "unexpected character while scanning integer literal");
        }
    }
    do
    {
        save_and_next();
    } while (ise(DIGIT()));
    if (is('e') || is('E'))
    {
        save_and_next();
        if (ise(PLUS()))
        {
            save_and_next();
        }
        if (!ise(DIGIT()))
        {
            if (ise(ERROR()))
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "read error while scanning integer literal");
            }
            else if (ise(END_OF_INPUT()))
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "premature end of input while scanning integer literal");
            }
            else
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "unexpected character while scanning integer literal");
            }
        }
        do
        {
            save_and_next();
        } while (ise(DIGIT()));
    }
    return Ego::Script::DDLToken(Ego::Script::DDLTokenKind::Integer, startLocation, get_lexeme_text());
}

Ego::Script::DDLToken ReadContext::parseNaturalLiteral()
{
    clear_lexeme_text();
    idlib::hll::location startLocation = get_location();
    if (ise(PLUS()))
    {
        save_and_next();
    }
    if (!ise(DIGIT()))
    {
        if (ise(ERROR()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "read error while scanning natural literal");
        }
        else if (ise(END_OF_INPUT()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "premature end of input while scanning natural literal");
        }
        else
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "unexpected character while scanning natural literal");
        }
    }
    do
    {
        save_and_next();
    } while (ise(DIGIT()));
    if (is('e') || is('E'))
    {
        save_and_next();
        if (ise(PLUS()))
        {
            save_and_next();
        }
        if (!ise(DIGIT()))
        {
            if (ise(ERROR()))
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "read error while scanning natural literal");
            }
            else if (ise(END_OF_INPUT()))
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "premature end of input while scanning natural literal");
            }
            else
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "unexpected character while scanning natural literal");
            }
        }
        do
        {
            save_and_next();
        } while (ise(DIGIT()));
    }
    return Ego::Script::DDLToken(Ego::Script::DDLTokenKind::Integer, startLocation, get_lexeme_text());
}

Ego::Script::DDLToken ReadContext::parseRealLiteral()
{
    clear_lexeme_text();
    idlib::hll::location startLocation = get_location();
    if (is('+') || is('-'))
    {
        save_and_next();
    }
    if (is('.'))
    {
        save_and_next();
        if (!ise(DIGIT()))
        {
            if (ise(ERROR()))
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "read error while scanning real literal");
            }
            else if (ise(END_OF_INPUT()))
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "premature end of input while scanning real literal");
            }
            else
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "unexpected character while scanning real literal");
            }
        }
        do
        {
            save_and_next();
        } while (ise(DIGIT()));
    }
    else if (ise(DIGIT()))
    {
        do
        {
            save_and_next();
        } while (ise(DIGIT()));
        if (is('.'))
        {
            save_and_next();
            while (ise(DIGIT()))
            {
                save_and_next();
            }
        }
    }
    if (is('e') || is('E'))
    {
        save_and_next();
        if (is('+') || is('-'))
        {
            save_and_next();
        }
        if (!ise(DIGIT()))
        {
            if (ise(ERROR()))
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "read error while scanning real literal exponent");
            }
            else if (ise(END_OF_INPUT()))
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "premature end of input while scanning real literal exponent");
            }
            else
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "unexpected character while scanning real literal exponent");
            }
        }
        do
        {
            save_and_next();
        } while (ise(DIGIT()));
    }
    return Ego::Script::DDLToken(Ego::Script::DDLTokenKind::Real, startLocation, get_lexeme_text());
}

std::string ReadContext::readStringLiteral() {
    skipWhiteSpaces();
    auto token = parseStringLiteral();
    return Ego::Script::DDLTokenDecoder<std::string>()(token);
}

char ReadContext::readCharacterLiteral() {
    skipWhiteSpaces();
    auto token = parseCharacterLiteral();
    return Ego::Script::DDLTokenDecoder<char>()(token);
}

signed int ReadContext::readIntegerLiteral() {
    skipWhiteSpaces();
    auto token = parseIntegerLiteral();
    return Ego::Script::DDLTokenDecoder<signed int>()(token);
}

unsigned int ReadContext::readNaturalLiteral() {
    skipWhiteSpaces();
    auto token = parseNaturalLiteral();
    return Ego::Script::DDLTokenDecoder<unsigned int>()(token);
}

float ReadContext::readRealLiteral() {
    skipWhiteSpaces();
    auto token = parseRealLiteral();
    return Ego::Script::DDLTokenDecoder<float>()(token);
}

#pragma pop_macro("ERROR")
