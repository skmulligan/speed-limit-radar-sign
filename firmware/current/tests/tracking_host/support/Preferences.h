#pragma once
#include <map>
#include <string>
#include <stdint.h>
class Preferences {
 public:
  inline static std::map<std::string,uint64_t> values;
  inline static bool failWrite=false;
  bool begin(const char*,bool){return true;}void end(){}
  uint64_t getULong64(const char* key,uint64_t fallback){auto p=values.find(key);return p==values.end()?fallback:p->second;}
  size_t putULong64(const char* key,uint64_t value){if(failWrite)return 0;values[key]=value;return 8;}
};
