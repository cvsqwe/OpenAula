#pragma once

#include <map>
#include <string>
#include <vector>


// minimal JSON reader for request bodies, responses are built by hand in main.cpp
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

    // returns Null on malformed input instead of throwing
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
