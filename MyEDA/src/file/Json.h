#pragma once
#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <climits>
#include <cctype>

// Small strict JSON reader for project files. No regex extraction or partial loads.
namespace ProjectJson {
struct Value {
    enum Kind { Null, Boolean, Number, String, Array, Object } kind=Null;
    int number=0;
    std::string text;
    std::vector<Value> items;
    std::map<std::string,Value> fields;
    const Value& at(const char* key) const {
        if(kind!=Object) throw std::runtime_error("object required");
        auto it=fields.find(key);
        if(it==fields.end()) throw std::runtime_error("missing field");
        return it->second;
    }
    int integer() const { if(kind!=Number) throw std::runtime_error("integer required"); return number; }
    const std::string& string() const { if(kind!=String) throw std::runtime_error("string required"); return text; }
    const std::vector<Value>& array() const { if(kind!=Array) throw std::runtime_error("array required"); return items; }
    bool has(const char* key) const { return fields.count(key)!=0; }
};
class Reader {
    const std::string& input;
    size_t pos=0;
    void fail() { throw std::runtime_error("invalid JSON"); }
    void space() { while(pos<input.size() && (input[pos]==' ' || input[pos]=='\n' || input[pos]=='\r' || input[pos]=='\t')) ++pos; }
    bool take(char c) { space(); if(pos<input.size() && input[pos]==c){++pos;return true;} return false; }
    unsigned hex4() {
        unsigned v=0;
        for(int i=0;i<4;++i) {
            if(pos>=input.size()) fail();
            char c=input[pos++]; unsigned n;
            if(c>='0' && c<='9') n=c-'0';
            else if(c>='a' && c<='f') n=c-'a'+10;
            else if(c>='A' && c<='F') n=c-'A'+10;
            else { fail(); return 0; }
            v=v*16+n;
        }
        return v;
    }
    void utf8(std::string& s,unsigned c) {
        if(c<0x80) s+=(char)c;
        else if(c<0x800){s+=(char)(0xc0|(c>>6));s+=(char)(0x80|(c&63));}
        else if(c<0x10000){s+=(char)(0xe0|(c>>12));s+=(char)(0x80|((c>>6)&63));s+=(char)(0x80|(c&63));}
        else{s+=(char)(0xf0|(c>>18));s+=(char)(0x80|((c>>12)&63));s+=(char)(0x80|((c>>6)&63));s+=(char)(0x80|(c&63));}
    }
    std::string quoted() {
        if(!take('"')) fail();
        std::string s;
        while(pos<input.size()) {
            unsigned char c=input[pos++];
            if(c=='"') return s;
            if(c<32) fail();
            if(c!='\\'){s+=(char)c;continue;}
            if(pos>=input.size()) fail();
            switch(input[pos++]) {
            case '"':s+='"';break;case '\\':s+='\\';break;case '/':s+='/';break;
            case 'b':s+='\b';break;case 'f':s+='\f';break;case 'n':s+='\n';break;case 'r':s+='\r';break;case 't':s+='\t';break;
            case 'u': {
                unsigned u=hex4();
                if(u>=0xd800 && u<=0xdbff) {
                    if(pos+2>input.size() || input[pos]!='\\' || input[pos+1]!='u') fail();
                    pos+=2;unsigned low=hex4();
                    if(low<0xdc00 || low>0xdfff) fail();
                    u=0x10000+((u-0xd800)<<10)+(low-0xdc00);
                } else if(u>=0xdc00 && u<=0xdfff) fail();
                if(!u) fail(); // Embedded NUL is not valid for project names.
                utf8(s,u);break;
            }
            default:fail();
            }
        }
        fail();return s;
    }
    Value value(int depth) {
        if(depth>64) fail();space();if(pos>=input.size()) fail();
        Value v;
        if(input[pos]=='{') {
            ++pos;v.kind=Value::Object;if(take('}')) return v;
            do { auto key=quoted();if(!take(':')) fail();auto child=value(depth+1);if(!v.fields.emplace(key,std::move(child)).second) fail(); } while(take(','));
            if(!take('}')) fail();return v;
        }
        if(input[pos]=='[') {
            ++pos;v.kind=Value::Array;if(take(']')) return v;
            do {v.items.push_back(value(depth+1));} while(take(','));
            if(!take(']')) fail();return v;
        }
        if(input[pos]=='"'){v.kind=Value::String;v.text=quoted();return v;}
        for(const auto& token : {std::string("true"),std::string("false"),std::string("null")}) {
            if(input.compare(pos,token.size(),token)==0){pos+=token.size();v.kind=token=="null"?Value::Null:Value::Boolean;v.number=token=="true";return v;}
        }
        size_t begin=pos;
        if(input[pos]=='-') ++pos;
        if(pos>=input.size() || !std::isdigit((unsigned char)input[pos])) fail();
        if(input[pos]=='0') ++pos;else while(pos<input.size() && std::isdigit((unsigned char)input[pos])) ++pos;
        long long n=std::stoll(input.substr(begin,pos-begin));
        if(n<INT_MIN || n>INT_MAX) fail();
        v.kind=Value::Number;v.number=(int)n;return v;
    }
public:
    explicit Reader(const std::string& s):input(s){}
    Value parse() {
        if(input.size()>10*1024*1024) fail();
        if(input.compare(0,3,"\xef\xbb\xbf")==0) pos=3;
        Value result=value(0);space();if(pos!=input.size()) fail();return result;
    }
};
inline std::string Quote(const std::string& text) {
    std::string s="\"";const char* hex="0123456789abcdef";
    for(unsigned char c:text) {
        if(c=='"' || c=='\\'){s+='\\';s+=(char)c;}
        else if(c<32){s+="\\u00";s+=hex[c>>4];s+=hex[c&15];}
        else s+=(char)c;
    }
    return s+"\"";
}
inline std::string Stringify(const Value& v) {
    switch(v.kind) {
    case Value::String:return Quote(v.text);
    case Value::Number:return std::to_string(v.number);
    case Value::Boolean:return v.number?"true":"false";
    case Value::Null:return "null";
    case Value::Array: {std::string s="[";for(size_t i=0;i<v.items.size();++i){if(i)s+=",";s+=Stringify(v.items[i]);}return s+"]";}
    case Value::Object:{std::string s="{";bool first=true;for(const auto& pair:v.fields){if(!first)s+=",";first=false;s+=Quote(pair.first)+":"+Stringify(pair.second);}return s+"}";}
    }
    return "null";
}

}
