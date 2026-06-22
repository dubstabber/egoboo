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

/// @file egolib/game/script_compile_parser.c
/// @brief parser_state_t token parsing, opcode emission, and syntax diagnostics.

#include "egolib/game/script_compile.h"
#include "egolib/game/game.h"
#include "egolib/game/egoboo.h"
#include "egolib/Log/_Include.hpp"
#include "egolib/Profiles/IProfileSystem.hpp"
#include "egolib/Script/CLogEntry.hpp"
#include "egolib/Core/StringUtilities.hpp"
#include "egolib/Profiles/_Include.hpp"

Ego::Script::PDLToken parser_state_t::parse_indention(script_info_t& script, line_scanner_state_t& state)
{
    auto source = state.scanWhiteSpaces();
    if (source.category() != Ego::Script::PDLTokenKind::Whitespace)
    {
        throw idlib::runtime_error(__FILE__, __LINE__, "internal error");
    }

    size_t indent = source.getValue();
    if (idlib::is_odd(indent))
    {
        Ego::Script::CLogEntry e(Log::Level::Message, __FILE__, __LINE__, __FUNCTION__, _token.get_start_location());
        e << "invalid indention - number of spaces must be even - \n"
          << " - \n`" << _lineBuffer.toString() << "`" << Log::EndOfEntry;
        Log::activeTarget() << e;
        _error = true;
    }

    indent >>= 1;
    if (indent > 15)
    {
        Ego::Script::CLogEntry e(Log::Level::Message, __FILE__, __LINE__, __FUNCTION__, _token.get_start_location());
        e << "invalid indention - too many spaces - \n"
          << " - \n`" << _lineBuffer.toString() << "`" << Log::EndOfEntry;
        Log::activeTarget() << e;
        _error = true;
        indent = 15;
    }
    auto token = Ego::Script::PDLToken(Ego::Script::PDLTokenKind::Indent, source.get_start_location(), source.getEndLocation());
    token.setValue(indent);
    return token;
}

//--------------------------------------------------------------------------------------------
Ego::Script::PDLToken parser_state_t::parse_token(ObjectProfile *ppro, script_info_t& script, line_scanner_state_t& state)
{
    /// @details This function tells what code is being indexed by read, it
    /// will return the next spot to read from and stick the code number in
    /// ptok->iIndex

    // Check bounds
    if (state.isEndOfInput())
    {
        return Ego::Script::PDLToken(Ego::Script::PDLTokenKind::EndOfLine, state.getLocation(), state.getLocation());
    }

    // Skip whitespaces.
    state.scanWhiteSpaces();

    // Stop if the line is empty.
    if (state.isEndOfInput())
    {
        return Ego::Script::PDLToken(Ego::Script::PDLTokenKind::EndOfLine, state.getLocation(), state.getLocation());
    }

    // initialize the word
    if (state.isDoubleQuote())
    {
        auto token = state.scanStringOrReference();
        if (token.category() == Ego::Script::PDLTokenKind::ReferenceLiteral)
        {
            // If it is a profile reference.

            // Invalid profile as default.
            token.setValue(INVALID_PRO_REF);
            // Convert reference to slot number.
            for (const auto& element : activeProfileSystem().getLoadedProfiles())
            {
                const auto& profile = element.second;
                if (profile == nullptr) continue;
                // Is this the object we are looking for?
                if (idlib::is_suffix(profile->getPathname(), token.get_lexeme()))
                {
                    token.setValue(profile->getSlotNumber().get());
                    break;
                }
            }

            // Do we need to load the object?
            if (!activeProfileSystem().isLoaded((PRO_REF)token.getValue()))
            {
                auto loadName = "mp_objects/" + token.get_lexeme();

                // Find first free slot number.
                for (PRO_REF ipro = MAX_IMPORT_PER_PLAYER * 4; ipro < INVALID_PRO_REF; ipro++)
                {
                    //skip loaded profiles
                    if (activeProfileSystem().isLoaded(ipro)) continue;

                    //found a free slot
                    token.setValue(activeProfileSystem().loadOneProfile(loadName, REF_TO_INT(ipro)).get());
                    if (token.getValue() == ipro) break;
                }
            }

            // Failed to load object!
            if (!activeProfileSystem().isLoaded((PRO_REF)token.getValue()))
            {
                Ego::Script::CLogEntry e(Log::Level::Message, __FILE__, __LINE__, __FUNCTION__, token.get_start_location());
                e << "failed to load object " << token.get_lexeme() << " - \n"
                    << " - \n`" << _lineBuffer.toString() << "`" << Log::EndOfEntry;
                Log::activeTarget() << e;
            }
            token.category(Ego::Script::PDLTokenKind::Constant);
        }
        else
        {
            // Add the string as a message message to the available messages of the object.
            token.setValue(ppro->addMessage(token.get_lexeme(), true));
            token.category(Ego::Script::PDLTokenKind::Constant);
            // Emit a warning that the string is empty.
            Ego::Script::CLogEntry e(Log::Level::Message, __FILE__, __LINE__, __FUNCTION__, token.get_start_location());
            e << "empty string literal\n" << Log::EndOfEntry;
            Log::activeTarget() << e;
        }
        return token;
    } else if (state.isOperator()) {
        auto token = state.scanOperator();
        switch (token.category())
        {
            case Ego::Script::PDLTokenKind::Plus: token.setValue(Ego::Script::ScriptOperators::OPADD); break;
            case Ego::Script::PDLTokenKind::Minus: token.setValue(Ego::Script::ScriptOperators::OPSUB); break;
            case Ego::Script::PDLTokenKind::And: token.setValue(Ego::Script::ScriptOperators::OPAND); break;
            case Ego::Script::PDLTokenKind::ShiftRight: token.setValue(Ego::Script::ScriptOperators::OPSHR); break;
            case Ego::Script::PDLTokenKind::ShiftLeft: token.setValue(Ego::Script::ScriptOperators::OPSHL); break;
            case Ego::Script::PDLTokenKind::Multiply: token.setValue(Ego::Script::ScriptOperators::OPMUL); break;
            case Ego::Script::PDLTokenKind::Divide: token.setValue(Ego::Script::ScriptOperators::OPDIV); break;
            case Ego::Script::PDLTokenKind::Modulus: token.setValue(Ego::Script::ScriptOperators::OPMOD); break;
            case Ego::Script::PDLTokenKind::Assign: token.setValue(-1); break;
            default: throw idlib::runtime_error(__FILE__, __LINE__, "internal error");
        };
        return token;
    } else if (state.is('[')) {
        auto token = state.scanIDSZ();
        token.category(Ego::Script::PDLTokenKind::Constant);
        IDSZ2 idsz = IDSZ2(token.get_lexeme());
        token.setValue(idsz.toUint32());
        return token;
    } else if (state.isDigit()) {
        auto token = state.scanNumericLiteral();
        token.category(Ego::Script::PDLTokenKind::Constant);
        int temporary;
        sscanf(token.get_lexeme().c_str(), "%d", &temporary);
        token.setValue(temporary);
        return token;
    } else if (state.is('_') || state.isAlphabetic()) {
        auto token = state.scanName();
        auto it = std::find_if(Opcodes.cbegin(), Opcodes.cend(), [&token](const auto& opcode) { return token.get_lexeme() == opcode.cName; });
        // We couldn't figure out what this is, throw out an error code
        if (it == Opcodes.cend())
        {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, state.getLocation(), "not an opcode");
        }
        token.setValue((*it).iValue);
        token.category((*it)._kind);
        return token;
    } else {
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::lexical, state.getLocation(), "unexpected symbol");
    }
}

//--------------------------------------------------------------------------------------------
void parser_state_t::emit_opcode(const Ego::Script::PDLToken& token, const BIT_FIELD highbits, script_info_t& script)
{
    BIT_FIELD loc_highbits = highbits;

    // Emit the opcode.
    if (Ego::Script::PDLTokenKind::Constant == token.category())
    {
        loc_highbits |= Instruction::FUNCTIONBITS;
        auto constantIndex = script._instructions.getConstantPool().getOrCreateConstant(token.getValue());
        script._instructions.append(Instruction(loc_highbits | constantIndex));
    }
    else if (Ego::Script::PDLTokenKind::Function == token.category())
    {
        loc_highbits |= Instruction::FUNCTIONBITS;
        auto constantIndex = script._instructions.getConstantPool().getOrCreateConstant(token.getValue());
        script._instructions.append(Instruction(loc_highbits | constantIndex));
    }
    else if (Ego::Script::PDLTokenKind::Variable == token.category())
    {
        auto constantIndex = script._instructions.getConstantPool().getOrCreateConstant(token.getValue());
        script._instructions.append(Instruction(loc_highbits | constantIndex));
    }
    else
    {
        /** @todo This is not an error of the syntactical analysis. */
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::syntactical, token.get_start_location(), "unsupported token");
    }
}

//--------------------------------------------------------------------------------------------

void parser_state_t::raise(bool raiseException, Log::Level level, const Ego::Script::PDLToken& received, const std::vector<Ego::Script::PDLTokenKind>& expected)
{
    Ego::Script::CLogEntry e(level, __FILE__, __LINE__, __FUNCTION__, received.get_start_location());
    if (expected.size() > 0)
    {
        e << "expected ";
        size_t index = 0;
        e << "`" << toString(expected[index]) << "`";
        index++;
        if (expected.size() == 2)
        {
            e << " or" << "`" << toString(expected[index]) << "`";
        }
        else if (expected.size() > 2)
        {
            for (; index < expected.size() - 1; index++)
            {
                e << ", " << "`" << toString(expected[index]) << "`";
            }
            e << ", or" << "`" << toString(expected[index]) << "`";
        }
        e << ", ";
    }
    e << "received " << "`" << toString(received.category()) << "`" << Log::EndOfEntry;
    Log::activeTarget() << e;
    if (raiseException)
    {
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::syntactical, received.get_start_location(), e.getText());
    }
}
