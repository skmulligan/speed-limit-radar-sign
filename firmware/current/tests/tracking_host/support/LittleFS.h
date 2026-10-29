#pragma once
// Host-only filesystem adapter; it models file operations, not ESP32 flash timing.
#include <string>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
#include <cstring>
class String:public std::string {
 public:using std::string::string;String(const std::string& s):std::string(s){}
 bool startsWith(const char* s)const{return rfind(s,0)==0;}
 bool endsWith(const char* s)const{size_t n=strlen(s);return size()>=n&&compare(size()-n,n,s)==0;}
};
class File {
 public:
  File()=default;
  explicit File(std::filesystem::path path):path_(path){
    if(std::filesystem::is_directory(path)){
      directory_=true;for(const auto& e:std::filesystem::directory_iterator(path))entries_.push_back(e.path());
    }else if(std::filesystem::exists(path))stream_=std::make_shared<std::ifstream>(path,std::ios::binary);
  }
  explicit operator bool()const{return directory_||bool(stream_&&stream_->is_open());}
  File openNextFile(){return position_<entries_.size()?File(entries_[position_++]):File();}
  String name()const{return path_.filename().string();}
  size_t size()const{return std::filesystem::file_size(path_);}
  bool seek(size_t offset){if(!stream_)return false;stream_->clear();stream_->seekg(offset);return bool(*stream_);}
  size_t read(uint8_t* p,size_t n){stream_->read(reinterpret_cast<char*>(p),n);return size_t(stream_->gcount());}
 private:std::filesystem::path path_;std::shared_ptr<std::ifstream> stream_;bool directory_=false;std::vector<std::filesystem::path> entries_;size_t position_=0;
};
struct HostLittleFS {
 std::filesystem::path root;size_t capacity=256*1024;bool mounted=true,failRemove=false;
 bool begin(bool,const char* path){root=path;return mounted&&std::filesystem::exists(root);}
 bool format(){std::filesystem::create_directories(root);mounted=true;return true;}
 File open(const char* path,const char* ="r"){return File(root/std::string(path).substr(1));}
 File open(const String& path,const char* mode="r"){return open(path.c_str(),mode);}
 size_t totalBytes()const{return capacity;}
 size_t usedBytes()const{size_t n=0;for(const auto&e:std::filesystem::directory_iterator(root))if(e.is_regular_file())n+=e.file_size();return n;}
 bool remove(const String& path){return !failRemove&&std::filesystem::remove(root/std::string(path).substr(1));}
};
inline HostLittleFS LittleFS;
