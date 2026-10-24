#pragma once
#include <cstring>
#include <map>
#include <string>
#include <vector>
namespace fakeNvs {
inline std::map<std::string,std::vector<char>> values;
inline bool failOpen=false,failWrite=false,failRead=false;
inline unsigned writes=0;
}
class Preferences {
public:
    bool begin(const char* name,bool=false) { space_=name;return !fakeNvs::failOpen; }
    void end() {}
    bool isKey(const char* key) {return fakeNvs::values.count(space_+"/"+key);}
    size_t getBytesLength(const char* key) {return fakeNvs::values[space_+"/"+key].size();}
    size_t getBytes(const char* key,void* data,size_t size) {
        const auto& value=fakeNvs::values[space_+"/"+key];
        if(fakeNvs::failRead||size!=value.size())return 0;
        std::memcpy(data,value.data(),size);return size;
    }
    size_t putBytes(const char* key,const void* data,size_t size) {
        if(fakeNvs::failWrite)return 0;
        const char* bytes=static_cast<const char*>(data);
        fakeNvs::values[space_+"/"+key]=std::vector<char>(bytes,bytes+size);
        ++fakeNvs::writes;return size;
    }
private:
    std::string space_;
};
