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

/// @file egolib/game/script_compile.c
/// @brief Implementation of the script compiler
/// @details

#include "egolib/game/script_compile.h"
#include "egolib/game/egoboo.h"
#include "egolib/strutil.h"  // CSTR_END
#include "egolib/Log/_Include.hpp"
#include "egolib/Profiles/IProfileSystem.hpp"
#include "egolib/Script/CLogEntry.hpp"
#include "egolib/Core/StringUtilities.hpp" // Ego::isspace, Ego::iscntrl, Ego::isalpha, Ego::isdigit
#include "egolib/Profiles/_Include.hpp" // ObjectProfile (complete type)

static bool load_ai_codes_vfs();

parser_state_t::parser_state_t()
	: _loadBuffer(), _token(), _lineBuffer()
{
	_line_count = 0;

    load_ai_codes_vfs();
    debug_script_file = vfs_openWrite("/debug/script_debug.txt");

    _error = false;
}

parser_state_t::~parser_state_t()
{
	_line_count = 0;

    vfs_close(debug_script_file);
    debug_script_file = nullptr;

    _error = false;
}

bool parser_state_t::get_error() const
{
    return _error;
}

void parser_state_t::clear_error()
{
    _error = false;
}

//--------------------------------------------------------------------------------------------

std::vector<opcode_data_t> Opcodes;

bool debug_scripts = false;
vfs_FILE *debug_script_file = NULL;

//--------------------------------------------------------------------------------------------

/// Emit a token to standard output in debug mode.
void print_token(const Ego::Script::PDLToken& token);
#if (DEBUG_SCRIPT_LEVEL > 2) && defined(_DEBUG)
    static void print_line();
#else
    #define print_line()
#endif

//--------------------------------------------------------------------------------------------
static bool isNewline(int x) {
    return ASCII_LINEFEED_CHAR == x || C_CARRIAGE_RETURN_CHAR == x;
}

bool parser_state_t::skipNewline(size_t& read, script_info_t& script) {
    size_t newread = read;
    if (newread < _loadBuffer.getSize()) {
        char current = _loadBuffer.get(newread);
        if (isNewline(current)) {
            newread++;
            if (newread < _loadBuffer.getSize()) {
                char old = current;
                current = _loadBuffer.get(newread);
                if (isNewline(current) && old != current) {
                    newread++;
                }
            }
        }
    }
    if (read != newread) {
        read = newread;
        return true;
    } else {
        return false;
    }
}
size_t parser_state_t::load_one_line( size_t read, script_info_t& script )
{
    /// @author ZZ
    /// @details This function loads a line into the line buffer

    char cTmp;

    // Parse to start to maintain indentation
	_lineBuffer.clear();

    // try to trap all end of line conditions so we can properly count the lines
    bool tabs_warning_needed = false;
    while ( read < _loadBuffer.getSize() )
    {
        if (skipNewline(read, script)) {
            _lineBuffer.clear();
            return read;
        }

        cTmp = _loadBuffer.get(read);
        if ( C_TAB_CHAR == cTmp )
        {
            tabs_warning_needed = true;
            cTmp = ' ';
        }

        if (!Ego::isspace(cTmp))
        {
            break;
        }

        _lineBuffer.append(' ');

        read++;
    }

    // Parse to comment or end of line
    bool foundtext = false;
    bool inside_string = false;
    while ( read < _loadBuffer.getSize() )
    {
        cTmp = _loadBuffer.get(read);

        // we reached endline
        if (isNewline(cTmp))
        {
            break;
        }

        // we reached a comment
        if ( '/' == cTmp && '/' == _loadBuffer.get(read + 1))
        {
            break;
        }

        // Double apostrophe indicates where the string begins and ends
        if ( C_DOUBLE_QUOTE_CHAR == cTmp )
        {
            inside_string = !inside_string;
        }

        if (!inside_string && Ego::iscntrl(cTmp))
        {
            // Convert control characters to whitespace
            cTmp = ' ';
        }

        // convert whitespace characters to
        if ( !isspace(( unsigned )cTmp ) || inside_string )
        {
            foundtext = true;

            _lineBuffer.append(cTmp);
        }

        read++;
    }

    if ( !foundtext )
    {
        _lineBuffer.clear();
    }

    if ( _lineBuffer.getSize() > 0  && tabs_warning_needed )
    {
        Ego::Script::CLogEntry e(Log::Level::Message, __FILE__, __LINE__, __FUNCTION__, _token.get_start_location());
		e << "compilation error - tab character used to define spacing will cause an error `"
		  << " - \n`" << _lineBuffer.toString() << "`" << Log::EndOfEntry;
		Log::activeTarget() << e;
    }

    // scan to the beginning of the next line
    while ( read < _loadBuffer.getSize() )
    {
        if (skipNewline(read, script)) {
            break;
        }
        read++;
    }

    return read;
}

//--------------------------------------------------------------------------------------------
// Note: line_scanner_state_t method bodies (the EgoScript lexical scanner) live in the sibling
// script_compile_lexer.c TU, while token parsing / opcode emission helpers live in
// script_compile_parser.c. The structs are declared in script_compile.h, so call-sites in this
// file see the full types via that header.
//--------------------------------------------------------------------------------------------

void parser_state_t::parse_line_by_line( ObjectProfile *ppro, script_info_t& script )
{
    /// @author ZF
    /// @details This parses an AI script line by line

    size_t read = 0;
    size_t line = 1;
    for (_token.set_start_location({script.getName(), 1}); read < _loadBuffer.getSize(); _token.set_start_location({script.getName(), _token.get_start_location().line_number()}))
    {
        read = load_one_line( read, script );
        if ( 0 == _lineBuffer.getSize() ) continue;

#if (DEBUG_SCRIPT_LEVEL > 2) && defined(_DEBUG)
        print_line();
#endif
        line_scanner_state_t state(&_lineBuffer, {script.getName(), line});

        //------------------------------
        // grab the first opcode

        _token = parse_indention(script, state);
        if (!_token.is_one_of(Ego::Script::PDLTokenKind::Indent))
        {
            this->raise(true, Log::Level::Error, _token, {Ego::Script::PDLTokenKind::Indent});
        }
        auto indent = _token.getValue();
        _token = parse_token(ppro, script, state);

        /* `function` */
        if ( _token.is_one_of(Ego::Script::PDLTokenKind::Function) )
        {
            if ( Ego::Script::ScriptFunctions::End == _token.getValue() && 0 == indent )
            {
                // Stop processing.
                break;
            }

            // Emit the instruction.
            emit_opcode( _token, SetDataBits(indent), script );

            // Space for the control(?) code.
            _token.setValue(0);
            emit_opcode( _token, 0, script );

        }
        /* `variable assignOperator expression` */
        else if ( _token.is_one_of(Ego::Script::PDLTokenKind::Variable) )
        {
            int operand_index;
            int operands = 0;

            // Emit the opcode and save a position for the operand count.
            emit_opcode( _token, SetDataBits(indent), script );
            _token.setValue(0);
            operand_index = script._instructions.getNumberOfInstructions();
            emit_opcode( _token, 0, script );
            _token = parse_token(ppro, script, state);

            // `assignOperator`
			if ( !_token.isAssignOperator() )
            {
                this->raise(true, Log::Level::Error, _token, {Ego::Script::PDLTokenKind::Assign});
            }
            _token = parse_token(ppro, script, state);

            auto isOperator = [](const Ego::Script::PDLToken& token)
            {
                return token.isOperator() && !token.isAssignOperator();
            };

            auto isOperand = [](const Ego::Script::PDLToken& token)
            {
                return token.is_one_of(Ego::Script::PDLTokenKind::Variable,
                                       Ego::Script::PDLTokenKind::Constant);
            };
            
            if (isOperand(_token))
            {
                emit_opcode(_token, 0, script);
                operands++;
                _token = parse_token(ppro, script, state);
            }
            // `unaryPlus|unaryMinus`
            else if (_token.is_one_of(Ego::Script::PDLTokenKind::Plus, Ego::Script::PDLTokenKind::Minus))
            {
                // We have some expression of the format `('+'|'-') ...` and transform this into
                // `0 ('+'|'-') ...`. This is as close as we can currently get to proper code.
                auto token = Ego::Script::PDLToken(Ego::Script::PDLTokenKind::Constant, _token.get_start_location(), _token.getEndLocation(), "0");
                token.setValue(0);
                emit_opcode(token, 0, script);
                operands++;
                // Do not proceed.
            }
            else
            {
                this->raise(true, Log::Level::Error, _token, {Ego::Script::PDLTokenKind::Plus, Ego::Script::PDLTokenKind::Minus,
                            Ego::Script::PDLTokenKind::Variable, Ego::Script::PDLTokenKind::Constant});
            }

            // `((operator - assignOperator) (constant|variable))*`
            while ( !_token.is_one_of(Ego::Script::PDLTokenKind::EndOfLine) )
            {
                // `operator`
                if (!isOperator(_token))
                {
                    this->raise(true, Log::Level::Warning, _token,
                                {Ego::Script::PDLTokenKind::Plus, Ego::Script::PDLTokenKind::Minus,
                                 Ego::Script::PDLTokenKind::Multiply, Ego::Script::PDLTokenKind::Divide,
                                 Ego::Script::PDLTokenKind::Modulus,
                                 Ego::Script::PDLTokenKind::And,
                                 Ego::Script::PDLTokenKind::ShiftLeft, Ego::Script::PDLTokenKind::ShiftRight});
                }

				auto opcode = _token.getValue(); // The opcode.

                // `(constant|variable)`
                _token = parse_token(ppro, script, state);
                if (!isOperand(_token))
                {
                    this->raise(true, Log::Level::Error, _token, {Ego::Script::PDLTokenKind::Constant, Ego::Script::PDLTokenKind::Variable});
                }

                emit_opcode( _token, SetDataBits(opcode), script );
                operands++;
                
                _token = parse_token( ppro, script, state);
            }
            script._instructions[operand_index].setBits(operands);
        }
        else
        {
            this->raise(true, Log::Level::Error, _token, {Ego::Script::PDLTokenKind::Function, Ego::Script::PDLTokenKind::Variable});
        }

        //
        line++;
    }

    _token.setValue(Ego::Script::ScriptFunctions::End);
    _token.category(Ego::Script::PDLTokenKind::Function);
    emit_opcode( _token, 0, script );
    _token.setValue(script._instructions.getNumberOfInstructions() + 1);
    emit_opcode( _token, 0, script );
}

//--------------------------------------------------------------------------------------------
uint32_t parser_state_t::jump_goto( int index, int index_end, script_info_t& script )
{
    /// @author ZZ
    /// @details This function figures out where to jump to on a fail based on the
    ///    starting location and the following code.  The starting location
    ///    should always be a function code with indentation

    auto value = script._instructions[index];
    index += 2;
    int targetindent = GetDataBits( value.getBits() );
    int indent = 100;

    while ( indent > targetindent && index < index_end )
    {
        value = script._instructions[index];
        indent = GetDataBits ( value.getBits() );
        if ( indent > targetindent )
        {
            // Was it a function
            if ( value.isInv() )
            {
                // Each function needs a jump
                index++;
                index++;
            }
            else
            {
                // Operations cover each operand
                index++;
                value = script._instructions[index]; //AisCompiled_buffer[index];
                index++;
                index += ( value.getBits() & 255 );
            }
        }
    }

    return std::min( index, index_end );
}

//--------------------------------------------------------------------------------------------
void parser_state_t::parse_jumps( script_info_t& script )
{
    /// @author ZZ
    /// @details This function sets up the fail jumps for the down and dirty code

    uint32_t index     = 0;
    uint32_t index_end = script._instructions.getNumberOfInstructions();

    auto value = script._instructions[index];
    while ( index < index_end )
    {
        value = script._instructions[index];

        // Was it a function
        if (value.isInv())
        {
            // Each function needs a jump
            auto iTmp = jump_goto( index, index_end, script );
            index++;
            script._instructions[index].setBits(iTmp);
            index++;
        }
        else
        {
            // Operations cover each operand
            index++;
            auto iTmp = script._instructions[index];
            index++;
			index += Ego::Math::clipBits<8>( iTmp.getBits() );
        }
    }
}

//--------------------------------------------------------------------------------------------
bool load_ai_codes_vfs()
{
    /// @author ZZ
    /// @details This function loads all of the function and variable names

	struct aicode_t
	{
		/// The kind.
		Ego::Script::PDLTokenKind _kind;
		/// The value of th constant.
		uint32_t _value;
		/// The name.
		const char *_name;
	};

	static const aicode_t AICODES[] =
	{
    #define Define(name) { Ego::Script::PDLTokenKind::Function, Ego::Script::ScriptFunctions::name, #name }, 
    #define DefineAlias(alias, name) { Ego::Script::PDLTokenKind::Function, Ego::Script::ScriptFunctions::alias, #alias },
    #include "egolib/Script/Functions.in"
    #undef DefineAlias
    #undef Define

    #define Define(value, name) { Ego::Script::PDLTokenKind::Constant, value, name },
    #include "egolib/Script/Constants.in"
    #undef Define

    #define Define(cName, eName) { Ego::Script::PDLTokenKind::Variable, Ego::Script::ScriptVariables::cName, eName },
    #define DefineAlias(cName, eName) { Ego::Script::PDLTokenKind::Variable, Ego::Script::ScriptVariables::cName, eName },
    #include "egolib/Script/Variables.in"
    #undef DefineAlias
    #undef Define
	};

    for (size_t i = 0, n = sizeof(AICODES) / sizeof(aicode_t); i < n; ++i)
    {
        Opcodes.push_back(opcode_data_t());
        Opcodes[i].cName = AICODES[i]._name;
        Opcodes[i]._kind = AICODES[i]._kind;
        Opcodes[i].iValue = AICODES[i]._value;
    }
    return true;
}

//--------------------------------------------------------------------------------------------
static bool load_ai_script_vfs0(parser_state_t& ps, const std::string& loadname, ObjectProfile *ppro, script_info_t& script)
{
	ps.clear_error();
	ps._line_count = 0;
	// Clear the buffer.
    ps._loadBuffer.clear();

    // Load the entire file.
    try {
        if (!vfs_exists(loadname)) {
            return false;
        }
        vfs_readEntireFile(loadname, [&ps](size_t numberOfBytes, const char *bytes) { ps._loadBuffer.append(bytes, numberOfBytes); });
    } catch (...) {
        return false;
    }
    // Assert proper encoding: The file may not contain zero terminators.
    for (size_t i = 0; i < ps._loadBuffer.getSize(); ++i) {
        if (CSTR_END == ps._loadBuffer.get(i)) {
            return false;
        }
    }
    // Compile into a scratch script and only publish it once parse_jumps has run.
    // Compiling in place would leave the caller holding a half-built instruction
    // stream whose function fail-jumps were never resolved if the parse throws
    // part way through, and such a stream is executable — every conditional in it
    // would fall through instead of skipping its body.
    script_info_t compiled;
    try {
        // save the filename for error logging
        compiled._name = loadname;

        // parse/compile the scripts
        ps.parse_line_by_line(ppro, compiled);

        // determine the correct jumps
        parser_state_t::parse_jumps(compiled);
    } catch (const idlib::hll::compilation_error& e) {
        // A syntax error in a present, readable file. Report it: the caller only
        // says "unable to load", which reads like a missing file and has hidden
        // real content defects (a five-character IDSZ silently cost the wizard
        // classes their entire script).
        Log::Entry entry(Log::Level::Warning, __FILE__, __LINE__, __FUNCTION__);
        entry << "unable to compile script file `" << loadname << "`: " << e.to_string() << Log::EndOfEntry;
        Log::activeTarget() << entry;
        return false;
    } catch (...) {
        return false;
    }

    // Fully compiled and jump-resolved: publish it.
    script = std::move(compiled);
	return true;
}
bool load_ai_script_vfs(parser_state_t& ps, const std::string& loadname, ObjectProfile *ppro, script_info_t& script)
{
	/// @author ZZ
	/// @details This function loads a script to memory

	if (!load_ai_script_vfs0(ps, loadname, ppro, script)) {
		// A file that is present but does not compile is a content defect, not a
		// benign absence; say which case this is, because the two used to be
		// indistinguishable here and that hid a broken script for years.
		const bool present = vfs_exists(loadname);
		Log::Entry e(present ? Log::Level::Warning : Log::Level::Info, __FILE__, __LINE__, __FUNCTION__);
		e << (present ? "script file `" : "no script file `") << loadname
		  << (present ? "` did not compile" : "`")
		  << " - loading default script `" << "mp_data/script.txt" << "` instead" << Log::EndOfEntry;
		Log::activeTarget() << e;
		if (!load_ai_script_vfs0(ps, "mp_data/script.txt", ppro, script)) {
			// Neither source produced a runnable script. Leave nothing behind: an
			// empty script is inert, whereas whatever a caller might otherwise
			// inherit here is not guaranteed to be jump-resolved.
			script._name = loadname;
			script._instructions.clear();
			return false;
		}
	}
	return true;
}

//--------------------------------------------------------------------------------------------

void print_token(const Ego::Script::PDLToken& token) {
#if (DEBUG_SCRIPT_LEVEL > 2) && defined(_DEBUG)
	std::cout << token;
#endif
}

#if (DEBUG_SCRIPT_LEVEL > 2) && defined(_DEBUG)
void print_line( parser_state_t * ps )
{
    int i;
    char cTmp;

    printf( "\n===========\n\tfile == \"%s\"\n\tline == %d\n", globalparsename, ps->token.iLine );

    printf( "\tline == \"" );

    for ( i = 0; i < ps->line_buffer_count; i++ )
    {
        cTmp = ps->line_buffer[i];
        if ( isprint( cTmp ) )
        {
            printf( "%c", cTmp );
        }
        else
        {
            printf( "\\%03d", cTmp );
        }
    };

    printf( "\", length == %d\n", ps->line_buffer_count );
}

#endif
