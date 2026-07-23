#include "Json.h"

#include <cctype>
#include <cstdlib>


namespace json
{

namespace
{

const Value NullValue{};

}


bool Value::has(const std::string& key) const
{
    return type_ == Type::Object && obj_.count(key) > 0;
}



const Value& Value::operator[](const std::string& key) const
{
    if(type_ != Type::Object)
        return NullValue;

    auto it = obj_.find(key);
    return it == obj_.end() ? NullValue : it->second;
}



// Simple recursive-descent parser over a std::string, tracking position
// with a single index. Malformed input just makes parse() bail out with
// whatever was decoded so far turning into a Null at the top - see
// Value::parse().
class Parser
{
public:

    explicit Parser(const std::string& text) : text_(text) {}

    bool parseValue(Value& out)
    {
        skipWhitespace();

        if(pos_ >= text_.size())
            return false;

        char c = text_[pos_];

        if(c == '{')  return parseObject(out);
        if(c == '[')  return parseArray(out);
        if(c == '"')  return parseString(out);
        if(c == 't' || c == 'f') return parseBool(out);
        if(c == 'n')  return parseNull(out);

        return parseNumber(out);
    }


private:

    const std::string& text_;
    size_t pos_ = 0;

    void skipWhitespace()
    {
        while(pos_ < text_.size() && std::isspace((unsigned char)text_[pos_]))
            pos_++;
    }

    bool consume(char expected)
    {
        skipWhitespace();

        if(pos_ >= text_.size() || text_[pos_] != expected)
            return false;

        pos_++;
        return true;
    }

    bool parseObject(Value& out)
    {
        if(!consume('{'))
            return false;

        out.type_ = Value::Type::Object;

        skipWhitespace();
        if(consume('}'))
            return true;

        while(true)
        {
            Value keyVal;

            if(!parseString(keyVal))
                return false;

            if(!consume(':'))
                return false;

            Value val;
            if(!parseValue(val))
                return false;

            out.obj_[keyVal.str_] = val;

            skipWhitespace();

            if(consume(','))
                continue;

            return consume('}');
        }
    }

    bool parseArray(Value& out)
    {
        if(!consume('['))
            return false;

        out.type_ = Value::Type::Array;

        skipWhitespace();
        if(consume(']'))
            return true;

        while(true)
        {
            Value val;

            if(!parseValue(val))
                return false;

            out.arr_.push_back(val);

            skipWhitespace();

            if(consume(','))
                continue;

            return consume(']');
        }
    }

    bool parseString(Value& out)
    {
        if(!consume('"'))
            return false;

        std::string result;

        while(pos_ < text_.size() && text_[pos_] != '"')
        {
            char c = text_[pos_++];

            if(c == '\\' && pos_ < text_.size())
            {
                char esc = text_[pos_++];

                switch(esc)
                {
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'u':
                        // Only handles the common BMP/ASCII case (four hex
                        // digits, no surrogate pairs) - sufficient for the
                        // key labels and profile names this API deals
                        // with.
                        if(pos_ + 4 <= text_.size())
                        {
                            int code = std::strtol(text_.substr(pos_, 4).c_str(), nullptr, 16);
                            pos_ += 4;

                            if(code < 0x80)
                                result += (char)code;
                            else
                                result += '?';
                        }
                        break;
                    default: result += esc; break;
                }
            }
            else
            {
                result += c;
            }
        }

        if(pos_ >= text_.size())
            return false;

        pos_++; // closing quote

        out.type_ = Value::Type::String;
        out.str_ = result;
        return true;
    }

    bool parseBool(Value& out)
    {
        if(text_.compare(pos_, 4, "true") == 0)
        {
            pos_ += 4;
            out.type_ = Value::Type::Bool;
            out.bool_ = true;
            return true;
        }

        if(text_.compare(pos_, 5, "false") == 0)
        {
            pos_ += 5;
            out.type_ = Value::Type::Bool;
            out.bool_ = false;
            return true;
        }

        return false;
    }

    bool parseNull(Value& out)
    {
        if(text_.compare(pos_, 4, "null") != 0)
            return false;

        pos_ += 4;
        out.type_ = Value::Type::Null;
        return true;
    }

    bool parseNumber(Value& out)
    {
        size_t start = pos_;

        if(pos_ < text_.size() && (text_[pos_] == '-' || text_[pos_] == '+'))
            pos_++;

        while(pos_ < text_.size() && (std::isdigit((unsigned char)text_[pos_]) || text_[pos_] == '.'
              || text_[pos_] == 'e' || text_[pos_] == 'E' || text_[pos_] == '-' || text_[pos_] == '+'))
            pos_++;

        if(pos_ == start)
            return false;

        out.type_ = Value::Type::Number;
        out.num_ = std::strtod(text_.substr(start, pos_ - start).c_str(), nullptr);
        return true;
    }
};



Value Value::parse(const std::string& text)
{
    Value out;
    Parser parser(text);

    if(!parser.parseValue(out))
        return Value{};

    return out;
}

}
