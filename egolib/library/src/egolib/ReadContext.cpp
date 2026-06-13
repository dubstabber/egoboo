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

/// @file egolib/ReadContext.cpp
/// @brief Implementation of ReadContext member functions

#include "egolib/fileutil.h"

#include "egolib/Script/DDLTokenDecoder.hpp"
#include "egolib/Log/_Include.hpp"
#include "egolib/strutil.h"
#include "egolib/platform.h"
#include "egolib/_math.h"

#pragma push_macro("ERROR")
#undef ERROR

ReadContext::ReadContext(const std::string& fileName) :
    Scanner(fileName)
{
}

ReadContext::~ReadContext()
{
}

float ReadContext::toReal() const
{
    float temporary;
    auto lexeme = get_lexeme_text();
    if (!idlib::hll::decoder<float>()(lexeme,temporary))
    {
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                            "unable to convert current lexeme `" + lexeme + "` into a value of type "
                                            "`float`");
    }
    return temporary;
}

void ReadContext::skipWhiteSpaces()
{
    if (ise(ERROR()))
    {
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                            "read error");
    }
    if (ise(END_OF_INPUT()))
    {
        return;
    }
    while (ise(WHITE_SPACE()))
    {
        next();
        if (ise(ERROR()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "read error");
        }
        if (ise(END_OF_INPUT()))
        {
            return;
        }
    }
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

IDSZ2 ReadContext::readIDSZ() {
    char c[4];
    skipWhiteSpaces();
    // `'['`
    if (!ise(LEFT_SQUARE_BRACKET()))
    {
        if (ise(ERROR()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "read error while scanning IDSZ");
        }
        else if (ise(END_OF_INPUT()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "premature end of input while scanning IDSZ");
        }
        else
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "unexpected character while scanning IDSZ");
        }
    }
    next();
    // `(<alphabetic>|<digit>|'_')^4`
    for (size_t i = 0; i < 4; ++i)
    {
        if (!ise(ALPHA()) && !ise(DIGIT()) && !ise(UNDERSCORE()))
        {
            if (ise(ERROR()))
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "read error while scanning IDSZ");
            }
            else if (ise(END_OF_INPUT()))
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "premature end of input while scanning IDSZ");
            }
            else
            {
                throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                    "unexpected character while scanning IDSZ");
            }
        }
        c[i] = static_cast<char>(current());
        next();
    }
    // `']'`
    if (!ise(RIGHT_SQUARE_BRACKET()))
    {
        if (ise(ERROR()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "read error while scanning IDSZ");
        }
        else if (ise(END_OF_INPUT()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "premature end of input while scanning IDSZ");
        }
        else
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "unexpected character while scanning IDSZ");
        }
    }
    next();
    return IDSZ2(c[0], c[1], c[2], c[3]);
}

//--------------------------------------------------------------------------------------------
bool ReadContext::skipToDelimiter(char delimiter, bool optional)
{
    while (true)
    {
        if (ise(ERROR()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "read error");
        }
        if (ise(END_OF_INPUT()))
        {
            if (optional)
            {
                return false;
            }
            else
            {
                throw Ego::Script::MissingDelimiterError(__FILE__, __LINE__, get_location(), delimiter);
            }
        }
        bool isDelimiter = is(delimiter);
        if (ise(NEW_LINE()))
        {
            new_line(nullptr);
        }
        else
        {
            next();
        }
        if (isDelimiter)
        {
            return true;
        }
    }
}

//--------------------------------------------------------------------------------------------

bool ReadContext::skipToColon(bool optional)
{
    return skipToDelimiter(':', optional);
}

//--------------------------------------------------------------------------------------------

std::string ReadContext::readToEndOfLine()
{
    skipWhiteSpaces();
    clear_lexeme_text();
    while (!ise(END_OF_INPUT()))
    {
        if (ise(NEW_LINE()))
        {
            new_line(nullptr);
            break;
        }
        save_and_next();
    }
    return get_lexeme_text();
}

std::string ReadContext::readSingleLineComment()
{
    skipWhiteSpaces();
    clear_lexeme_text();
    if (!ise(SLASH()))
    {
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                            "unexpected character while scanning single line comment");
    }
    next();
    if (!ise(SLASH()))
    {
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                            "unexpected character while scanning single line comment");
    }
    next();
    skipWhiteSpaces();
    while (!ise(END_OF_INPUT()))
    {
        if (ise(ERROR()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "read error while scanning single line comment");
        }
        if (ise(NEW_LINE()))
        {
            new_line(nullptr);
            break;
        }
        save_and_next();
    }
    return get_lexeme_text();
}

char ReadContext::readPrintable()
{
    skipWhiteSpaces();
    if (ise(END_OF_INPUT()) || ise(ERROR()))
    {
        if (ise(ERROR()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "read error while scanning printable character");
        }
        else
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "premature end of input while scanning printable character");
        }
    }
    if (!ise(ALPHA()) && !ise(DIGIT()) && !ise(EXCLAMATION_MARK()) && !ise(QUESTION_MARK()) && !ise(EQUAL()))
    {
        if (ise(ERROR()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "read error while scanning printable character");
        }
        else if (ise(END_OF_INPUT()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "premature end of input while scanning a printable character");
        }
        else
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "unexpected character while scanning a printable characters");
        }
    }
    char tmp = static_cast<char>(current());
    next();
    return tmp;
}


//--------------------------------------------------------------------------------------------

void ReadContext::readName0()
{
    if (!ise(ALPHA()) && !ise(UNDERSCORE()))
    {
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                            "invalid name");
    }
    do
    {
        save_and_next();
    } while (ise(ALPHA()) || ise(DIGIT()) || ise(UNDERSCORE()));
}

void ReadContext::readOldString0()
{
    static const auto p = idlib::parsing_expression::ordered_choice(WHITE_SPACE(), NEW_LINE(), END_OF_INPUT());
    if (ise(p))
    {
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                            "invalid old string");
    }
    do
    {
        save_and_next();
    } while (!ise(p));
}

std::string ReadContext::readOldString()
{
    skipWhiteSpaces();
    clear_lexeme_text();
    readOldString0();
    return get_lexeme_text();
}

std::string ReadContext::readName()
{
    skipWhiteSpaces();
    clear_lexeme_text();
    readName0();
    return get_lexeme_text();
}

void ReadContext::readReference0()
{
    if (!is('%'))
    {
        if (ise(ERROR()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "read error while scanning reference literal");
        }
        else if (ise(END_OF_INPUT()))
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "premature end of input while scanning reference literal");
        }
        else
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                                "unexpected character while scanning reference literal");
        }
    }
    save_and_next();
    readName0();
}

std::string ReadContext::readReference()
{
    skipWhiteSpaces();
    clear_lexeme_text();
    readReference0();
    return get_lexeme_text();
}

bool ReadContext::readBool()
{
    std::string name = readName();
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    if (name == "true" || name == "t")
    {
        return true;
    }
    else if (name == "false" || name == "f")
    {
        return false;
    }
    else
    {
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, get_location(),
                                            "unexpected character while scanning boolean literal");
    }
}

#pragma pop_macro("ERROR")
