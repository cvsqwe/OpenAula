#pragma once

#include <map>
#include <string>
#include <vector>


// Minimal JSON reader for parsing HTTP request bodies sent by web/app.js.
// Deliberately not a general-purpose library (no comments/trailing-comma
// support, no streaming) - just enough to decode the small flat objects
// this bridge's API actually receives. Output JSON (responses) is built
// by hand in main.cpp instead, matching the plain string-building style
// core/StateFormat.cpp already uses.
namespace json
{

class Value
{
public:

    enum class Type { Null, Bool, Number, String, Array, Object };


    Type type() const { return type_; }

    double asNumber(double fallback = 0.0) const { return type_ == Type::Number ? num_ : fallback; }
    std::string asString(const std::string& fallback = "") const { return type_ == Type::String ? str_ : fallback; }
    bool asBool(bool fallback = false) const { return type_ == Type::Bool ? bool_ : fallback; }

    bool has(const std::string& key) const;
    const Value& operator[](const std::string& key) const;

    const std::vector<Value>& items() const { return arr_; }

    // Parses `text` as a single JSON value. On any malformed input, returns
    // a Null value rather than throwing - callers just get fallback
    // defaults for missing/bad fields instead of the whole request 500ing.
    static Value parse(const std::string& text);


private:

    Type type_ = Type::Null;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    std::vector<Value> arr_;
    std::map<std::string, Value> obj_;

    friend class Parser;
};

}
