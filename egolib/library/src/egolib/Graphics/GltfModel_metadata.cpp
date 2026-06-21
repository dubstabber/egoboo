#include "egolib/Graphics/GltfModel_internal.hpp"

#include <cgltf.h>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace
{

using Ego::Graphics::AnimationMetadataInput;
using Ego::Graphics::GltfModelDetail::FramePlan;
using Ego::Graphics::ModelAnimationMetadata;

struct JsonValue
{
    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;
};

class JsonReader
{
public:
    explicit JsonReader(const char* text) :
        _text(text ? text : ""),
        _pos(0)
    {}

    bool parse(JsonValue& value)
    {
        skipWhitespace();
        if (!parseValue(value))
        {
            return false;
        }
        skipWhitespace();
        return _pos == _text.size();
    }

private:
    void skipWhitespace()
    {
        while (_pos < _text.size() && std::isspace(static_cast<unsigned char>(_text[_pos])))
        {
            ++_pos;
        }
    }

    bool consume(char expected)
    {
        skipWhitespace();
        if (_pos >= _text.size() || _text[_pos] != expected)
        {
            return false;
        }
        ++_pos;
        return true;
    }

    bool parseValue(JsonValue& value)
    {
        skipWhitespace();
        if (_pos >= _text.size())
        {
            return false;
        }

        switch (_text[_pos])
        {
            case 'n': return parseLiteral("null", JsonValue::Type::Null, value);
            case 't':
                if (!parseLiteral("true", JsonValue::Type::Bool, value))
                {
                    return false;
                }
                value.boolean = true;
                return true;
            case 'f':
                if (!parseLiteral("false", JsonValue::Type::Bool, value))
                {
                    return false;
                }
                value.boolean = false;
                return true;
            case '"': return parseStringValue(value);
            case '[': return parseArray(value);
            case '{': return parseObject(value);
            default: return parseNumber(value);
        }
    }

    bool parseLiteral(const char* literal, JsonValue::Type type, JsonValue& value)
    {
        const size_t length = std::strlen(literal);
        if (_text.compare(_pos, length, literal) != 0)
        {
            return false;
        }
        _pos += length;
        value = JsonValue();
        value.type = type;
        return true;
    }

    bool parseString(std::string& out)
    {
        if (_pos >= _text.size() || _text[_pos] != '"')
        {
            return false;
        }
        ++_pos;

        std::string result;
        while (_pos < _text.size())
        {
            const char c = _text[_pos++];
            if (c == '"')
            {
                out = result;
                return true;
            }
            if (c != '\\')
            {
                result.push_back(c);
                continue;
            }
            if (_pos >= _text.size())
            {
                return false;
            }

            const char escaped = _text[_pos++];
            switch (escaped)
            {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case 'u':
                    if (_pos + 4 > _text.size())
                    {
                        return false;
                    }
                    result.push_back('?');
                    _pos += 4;
                    break;
                default:
                    return false;
            }
        }

        return false;
    }

    bool parseStringValue(JsonValue& value)
    {
        value = JsonValue();
        value.type = JsonValue::Type::String;
        return parseString(value.text);
    }

    bool parseNumber(JsonValue& value)
    {
        const char* begin = _text.c_str() + _pos;
        char* end = nullptr;
        const double number = std::strtod(begin, &end);
        if (begin == end)
        {
            return false;
        }

        _pos += static_cast<size_t>(end - begin);
        value = JsonValue();
        value.type = JsonValue::Type::Number;
        value.number = number;
        return true;
    }

    bool parseArray(JsonValue& value)
    {
        if (!consume('['))
        {
            return false;
        }

        value = JsonValue();
        value.type = JsonValue::Type::Array;
        skipWhitespace();
        if (_pos < _text.size() && _text[_pos] == ']')
        {
            ++_pos;
            return true;
        }

        while (true)
        {
            JsonValue element;
            if (!parseValue(element))
            {
                return false;
            }
            value.array.push_back(std::move(element));

            skipWhitespace();
            if (_pos < _text.size() && _text[_pos] == ']')
            {
                ++_pos;
                return true;
            }
            if (!consume(','))
            {
                return false;
            }
        }
    }

    bool parseObject(JsonValue& value)
    {
        if (!consume('{'))
        {
            return false;
        }

        value = JsonValue();
        value.type = JsonValue::Type::Object;
        skipWhitespace();
        if (_pos < _text.size() && _text[_pos] == '}')
        {
            ++_pos;
            return true;
        }

        while (true)
        {
            skipWhitespace();
            std::string key;
            if (!parseString(key) || !consume(':'))
            {
                return false;
            }

            JsonValue member;
            if (!parseValue(member))
            {
                return false;
            }
            value.object.emplace(std::move(key), std::move(member));

            skipWhitespace();
            if (_pos < _text.size() && _text[_pos] == '}')
            {
                ++_pos;
                return true;
            }
            if (!consume(','))
            {
                return false;
            }
        }
    }

private:
    std::string _text;
    size_t _pos;
};

const JsonValue* member(const JsonValue& value, const std::string& name)
{
    if (value.type != JsonValue::Type::Object)
    {
        return nullptr;
    }

    const auto it = value.object.find(name);
    return it == value.object.end() ? nullptr : &it->second;
}

bool integerValue(const JsonValue& value, int& out)
{
    if (value.type != JsonValue::Type::Number)
    {
        return false;
    }

    const double rounded = std::round(value.number);
    if (std::fabs(value.number - rounded) > 0.000001)
    {
        return false;
    }
    if (rounded < static_cast<double>(std::numeric_limits<int>::min()) ||
        rounded > static_cast<double>(std::numeric_limits<int>::max()))
    {
        return false;
    }

    out = static_cast<int>(rounded);
    return true;
}

void installDefaultMetadata(std::vector<FramePlan>& frames, AnimationMetadataInput& metadata)
{
    frames.clear();
    frames.push_back(FramePlan());
    frames.back().name = "DA";
    frames.back().meshIndex = 0;
    frames.back().framefx = EMPTY_BIT_FIELD;

    metadata = AnimationMetadataInput();
    metadata.actions[ACTION_DA].present = true;
    metadata.actions[ACTION_DA].start = 0;
    metadata.actions[ACTION_DA].end = 0;
}

bool parseActionName(const std::string& name, ModelAction& action)
{
    action = ModelAnimationMetadata::stringToAction(name);
    return action != ACTION_COUNT;
}

} // namespace

namespace Ego
{
namespace Graphics
{
namespace GltfModelDetail
{

bool parseMetadataExtras(const cgltf_data& data,
                         std::vector<FramePlan>& frames,
                         AnimationMetadataInput& metadata,
                         std::string& error)
{
    if (!data.extras.data || data.extras.data[0] == '\0')
    {
        installDefaultMetadata(frames, metadata);
        return true;
    }

    JsonValue extras;
    if (!JsonReader(data.extras.data).parse(extras))
    {
        error = "invalid extras JSON";
        return false;
    }

    const JsonValue* egoboo = member(extras, "egoboo");
    if (!egoboo)
    {
        installDefaultMetadata(frames, metadata);
        return true;
    }
    if (egoboo->type != JsonValue::Type::Object)
    {
        error = "extras.egoboo must be an object";
        return false;
    }

    int version = 0;
    const JsonValue* versionValue = member(*egoboo, "version");
    if (!versionValue || !integerValue(*versionValue, version) || version != 1)
    {
        error = "extras.egoboo.version must be 1";
        return false;
    }

    const JsonValue* framesValue = member(*egoboo, "frames");
    if (!framesValue || framesValue->type != JsonValue::Type::Array || framesValue->array.empty())
    {
        error = "extras.egoboo.frames must be a non-empty array";
        return false;
    }

    frames.clear();
    for (const JsonValue& frameValue : framesValue->array)
    {
        if (frameValue.type != JsonValue::Type::Object)
        {
            error = "extras.egoboo.frames entries must be objects";
            return false;
        }

        const JsonValue* nameValue = member(frameValue, "name");
        const JsonValue* meshValue = member(frameValue, "mesh");
        if (!nameValue || nameValue->type != JsonValue::Type::String || !meshValue)
        {
            error = "extras.egoboo frame entries require string name and numeric mesh";
            return false;
        }

        int mesh = 0;
        if (!integerValue(*meshValue, mesh) || mesh < 0)
        {
            error = "extras.egoboo frame mesh must be a non-negative integer";
            return false;
        }

        BIT_FIELD framefx = EMPTY_BIT_FIELD;
        if (const JsonValue* framefxValue = member(frameValue, "framefx"))
        {
            int framefxInt = 0;
            if (!integerValue(*framefxValue, framefxInt) || framefxInt < 0)
            {
                error = "extras.egoboo framefx must be a non-negative integer";
                return false;
            }
            framefx = static_cast<BIT_FIELD>(framefxInt);
        }

        FramePlan frame;
        frame.name = nameValue->text;
        frame.meshIndex = static_cast<std::size_t>(mesh);
        frame.framefx = framefx;
        frames.push_back(std::move(frame));
    }

    metadata = AnimationMetadataInput();
    const JsonValue* actionsValue = member(*egoboo, "actions");
    if (!actionsValue || actionsValue->type != JsonValue::Type::Object)
    {
        error = "extras.egoboo.actions must be an object";
        return false;
    }

    for (const auto& actionEntry : actionsValue->object)
    {
        ModelAction action = ACTION_COUNT;
        if (!parseActionName(actionEntry.first, action))
        {
            error = "extras.egoboo.actions contains an unknown action `" + actionEntry.first + "`";
            return false;
        }

        const JsonValue& range = actionEntry.second;
        if (range.type != JsonValue::Type::Array || range.array.size() != 2)
        {
            error = "extras.egoboo action ranges must be two-element arrays";
            return false;
        }

        int start = 0;
        int end = 0;
        if (!integerValue(range.array[0], start) || !integerValue(range.array[1], end) ||
            start < 0 || end < start || static_cast<std::size_t>(end) >= frames.size())
        {
            error = "extras.egoboo action range is outside the frame table";
            return false;
        }

        metadata.actions[action].present = true;
        metadata.actions[action].start = start;
        metadata.actions[action].end = end;
    }

    if (const JsonValue* healAliasesValue = member(*egoboo, "healAliases"))
    {
        if (healAliasesValue->type != JsonValue::Type::Array)
        {
            error = "extras.egoboo.healAliases must be an array";
            return false;
        }

        for (const JsonValue& aliasValue : healAliasesValue->array)
        {
            if (aliasValue.type != JsonValue::Type::Array || aliasValue.array.size() != 2 ||
                aliasValue.array[0].type != JsonValue::Type::String ||
                aliasValue.array[1].type != JsonValue::Type::String)
            {
                error = "extras.egoboo.healAliases entries must be string pairs";
                return false;
            }

            ModelAction from = ACTION_COUNT;
            ModelAction to = ACTION_COUNT;
            if (!parseActionName(aliasValue.array[0].text, from) || !parseActionName(aliasValue.array[1].text, to))
            {
                error = "extras.egoboo.healAliases contains an unknown action";
                return false;
            }

            metadata.healAliases.emplace_back(from, to);
        }
    }

    return true;
}

} // namespace GltfModelDetail
} // namespace Graphics
} // namespace Ego
