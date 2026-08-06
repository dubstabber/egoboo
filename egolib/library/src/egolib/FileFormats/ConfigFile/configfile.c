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

/// @file egolib/FileFormats/ConfigFile/configfile.c
/// @brief Parsing and unparsing of configuration files

/**
 * @remark
 *  A configuration file contains sections which themselves contain key-value pairs.
 *  </br>
 *  A section name is contained between <tt>{}</tt>.
 *  Example: <tt>{Section Name}</tt>
 *  </br>
 *  A key-value pair has a key, a string value and an optional commentary.
 *  The key is contained between <tt>[]</tt>.
 *  Example: <tt>[Key Name]</tt>
 *  </br>
 *  The value is contained between <tt>"</tt>.
 *  Example: <tt>"true"</tt> or <tt>"Hello, World!"</tt> or <tt>"0.1"</tt>.
 *  To include a <tt>"</tt> in a string value, prefix the <tt>"</tt> by a <tt>\</tt>.
 *  To include a <tt>\</tt> in a string value, double the <tt>\</tt>.
 *  Example: <tt>"\\"</tt>.
 *  </br>
 *  A commentary begins with "//" and extends to the end of the line and is assigned to the key-value pair succeeding the it.
 */
/**
 * Example configuration file.:
 * @code
 * {Section 1}
 * // This is the comment for [Key1] : "TRUE".
 * [Key1] : "TRUE" // This is a commentary
 * // This is the comment for [Key2] : "Hello \"MAN\""
 * [Key2] : "Hello \"MAN\""
 * // This line and the next line are the comment for [Key2] : "\\xxx"
 * // which will become "\xxx".
 * [Key3] : "\\xxx"
 * @endcode
 */
/**
 * @bug
 *  Multiple section with the same name will be loaded and saved but only the first
 *  one will be looked for value. Should not load sections with same name.
 */
#include "egolib/FileFormats/ConfigFile/configfile.h"
#include "egolib/Log/_Include.hpp"

#pragma push_macro("ERROR")
#undef ERROR

//--------------------------------------------------------------------------------------------

namespace {

/// @brief Report a configuration-file diagnostic.
/// @param file, line the C/C++ source location of the failing check
/// @param fileName the name of the configuration file the diagnostic refers to
/// @param detail the diagnostic text
/// @remark
///  These diagnostics used to be written to @a stderr. They are routed through the log target
///  instead, but via Log::tryActiveTarget() rather than Log::activeTarget(): the latter throws
///  std::logic_error when no log target exists, and the parser/unparser are expected to signal
///  failure by return value only. Same guarded idiom as egolib/Graphics/GltfModel.cpp.
/// @remark
///  The source location is taken as a parameter rather than read from __FILE__/__LINE__ here:
///  Log::Entry renders it as the "<file>:<line>:" prefix of the entry, so evaluating it inside
///  this helper would make every diagnostic claim to come from this one line.
void logConfigFileWarning(const char *file, int line, const std::string& fileName, const char *detail)
{
    if (Log::Target* target = Log::tryActiveTarget())
    {
        *target << Log::Entry::create(Log::Level::Warning, file, line,
                                      fileName, ": ", detail, Log::EndOfEntry);
    }
}

} // namespace

/// @brief Report a configuration-file diagnostic from the calling source location.
#define LOG_CONFIG_FILE_WARNING(FILE_NAME, DETAIL) \
    logConfigFileWarning(__FILE__, __LINE__, (FILE_NAME), (DETAIL))

//--------------------------------------------------------------------------------------------

ConfigCommentLine::ConfigCommentLine(const std::string& text) :
    m_text(text)
{}

ConfigCommentLine::~ConfigCommentLine()
{}

const std::string& ConfigCommentLine::getText() const
{
    return m_text;
}

//--------------------------------------------------------------------------------------------

ConfigEntry::ConfigEntry(const idlib::hll::qualified_name& qualifiedName, const std::string& value) :
    m_qualifiedName(qualifiedName), m_value(value), m_commentLines()
{}

ConfigEntry::~ConfigEntry()
{}

const idlib::hll::qualified_name& ConfigEntry::getQualifiedName() const
{
    return m_qualifiedName;
}

const std::string& ConfigEntry::getValue() const
{
    return m_value;
}

const std::vector<ConfigCommentLine>& ConfigEntry::getCommentLines() const
{
    return m_commentLines;
}

//--------------------------------------------------------------------------------------------

bool ConfigFileParser::skipWhiteSpaces()
{
    while (ise(WHITE_SPACE()))
    {
        next();
    }
    return true;
}

//--------------------------------------------------------------------------------------------

bool ConfigFileParser::parseFile(std::shared_ptr<ConfigFile> target)
{
    _currentQualifiedName = nullptr;
    _currentValue = nullptr;
    while (!ise(END_OF_INPUT()))
    {
        if (ise(NEW_LINE()))
        {
            new_lines(nullptr);
        }
        else if (ise(WHITE_SPACE()))
        {
            skipWhiteSpaces();
        }
        else if (is('/'))
        {
            std::string comment;
            parseComment(comment);
            _commentLines.push_back(comment);
        }
        else if (!parseEntry(target))
        {
            return false;
        }
    }
    return true;
}

bool ConfigFileParser::parseEntry(std::shared_ptr<ConfigFile> target)
{
    if (!parseQualifiedName())
    {
        return false;
    }
    if (!skipWhiteSpaces())
    {
        return false;
    }
    if (!is(':'))
    {
        LOG_CONFIG_FILE_WARNING(get_file_name(), "invalid key-value pair");
        return false;
    }
    next();
    if (!skipWhiteSpaces())
    {
        return false;
    }
    if (!parseValue())
    {
        return false;
    }
    target->set(*_currentQualifiedName, *_currentValue);
    // Throw current qualified name and current value away.
    _currentQualifiedName.reset(nullptr);
    _currentValue.reset(nullptr);
    return true;
}

bool ConfigFileParser::parseComment(std::string& comment)
{
    assert(is('/'));
    next();
    if (!is('/'))
    {
        LOG_CONFIG_FILE_WARNING(get_file_name(), "invalid comment");
        return false;
    }
    next();
    clear_lexeme_text();
    while (!ise(END_OF_INPUT()) && !ise(ERROR()) && !ise(NEW_LINE()))
    {
        save_and_next();
    }
    new_lines(nullptr);
    if (ise(ERROR()))
    {
        LOG_CONFIG_FILE_WARNING(get_file_name(), "error while reading file");
        return false;
    }
    comment = get_lexeme_text();
    return true;
}

bool ConfigFileParser::parseName()
{
    assert(is('_') || ise(ALPHA()));
    while (is('_'))
    {
        save_and_next();
    }
    if (!ise(ALPHA()))
    {
        LOG_CONFIG_FILE_WARNING(get_file_name(), "invalid name");
        return false;
    }
    do
    {
        save_and_next();
    } while (ise(ALPHA()) || ise(DIGIT()) || is('_'));
    return true;
}

bool ConfigFileParser::parseQualifiedName()
{
    assert(is('_') || ise(ALPHA()));
    clear_lexeme_text();
    if (!parseName())
    {
        return false;
    }
    while (is('.'))
    {
        save_and_next();
        if (!parseName())
        {
            return false;
        }
    }
    _currentQualifiedName.reset(new idlib::hll::qualified_name(get_lexeme_text()));

    return true;
}

bool ConfigFileParser::parseValue()
{
    assert(is('"'));
    next();
    clear_lexeme_text();
    while (!ise(END_OF_INPUT()) && !is('"') && !ise(NEW_LINE()))
    {
        save_and_next();
    }
    if (!is('"'))
    {
        LOG_CONFIG_FILE_WARNING(get_file_name(), "invalid value");
        return false;
    }
    next();
    _currentValue.reset(new std::string(get_lexeme_text()));
    return true;
}

ConfigFileParser::ConfigFileParser(const std::string& fileName) :
    Scanner(fileName), _currentQualifiedName(nullptr), _currentValue(nullptr)
{}

ConfigFileParser::~ConfigFileParser()
{}

std::shared_ptr<ConfigFile> ConfigFileParser::parse()
{
    try
    {
        auto target = std::make_shared<ConfigFile>(get_file_name());
        if (!parseFile(target))
        {
            return nullptr;
        }
        return target;
    }
    catch (...)
    {
        // Report before discarding: without this entry the caller sees nothing but a null
        // result. What is caught and what is returned are unchanged.
        //
        // The routes that reach here are allocation failures - from std::make_shared<ConfigFile>
        // above, from AbstractConfigFile::set, or from the lexeme buffer. The
        // idlib::hll::compilation_error that idlib::hll::qualified_name's constructor raises for
        // a malformed name is NOT among them: parseName accepts `'_'* alpha (alpha|digit|'_')*`
        // joined by '.', a strict subset of the qualified_name grammar
        // `('_'|alpha)('_'|alpha|digit)*` joined by '.', so every lexeme that reaches that
        // constructor validates.
        //
        // The report itself allocates (Log::Entry holds an std::ostringstream), so it is
        // guarded: parse() must return nullptr on every failure, including an std::bad_alloc
        // that would otherwise be replaced by a fresh one thrown out of the handler.
        try
        {
            LOG_CONFIG_FILE_WARNING(get_file_name(), "aborted by an exception while parsing");
        }
        catch (...)
        {
        }
        return nullptr;
    }
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

bool ConfigFileUnParser::unparse(std::shared_ptr<ConfigEntry> entry)
{
    vfs_printf(_target,"%s : \"%s\"\n", entry->getQualifiedName().string().c_str(),
                                        entry->getValue().c_str());
    return true;
}

ConfigFileUnParser::ConfigFileUnParser() :
    _target(nullptr), _source(nullptr)
{}

ConfigFileUnParser::~ConfigFileUnParser()
{}

bool ConfigFileUnParser::unparse(std::shared_ptr<ConfigFile> source)
{
    try
    {
        _source = source;
        _target = vfs_openWrite(_source->getFileName().c_str());
        if (!_target)
        {
            LOG_CONFIG_FILE_WARNING(_source->getFileName(), "unable to open file for writing");
            _source = nullptr;
            return false;
        }
        // Sort the entries by their qualified names.
        std::vector<std::shared_ptr<ConfigEntry>> entries;
        for (const auto& entry : *_source)
        {
            entries.push_back(entry);
        }
        std::sort(entries.begin(), entries.end(),
             [] (const std::shared_ptr<ConfigEntry>& a, const std::shared_ptr<ConfigEntry>& b)
                {
                    return std::less<idlib::hll::qualified_name>()(a->getQualifiedName(), b->getQualifiedName());
                }
            );
        for (const auto& entry : entries)
        {
            unparse(entry);
        }
        _source = nullptr;
        vfs_close(_target);
        _target = nullptr;
    }
    catch (...)
    {
        _source = nullptr;
        if (_target)
        {
            vfs_close(_target);
            _target = nullptr;
        }
        return false;
    }
    return true;
}

#pragma pop_macro("ERROR")
