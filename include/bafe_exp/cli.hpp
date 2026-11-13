#pragma once
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace bafe_exp {
class Cli {
public:
    Cli(int argc,char**argv){
        for(int i=1;i<argc;++i){
            std::string k=argv[i];
            if(k.rfind("--",0)!=0) throw std::invalid_argument("expected --option, got "+k);
            k=k.substr(2);
            std::string v="true";
            if(i+1<argc && std::string(argv[i+1]).rfind("--",0)!=0) v=argv[++i];
            values_[k]=v;
        }
    }
    bool has(const std::string&k)const{return values_.count(k)!=0;}
    std::string get(const std::string&k,const std::string&d="")const{auto it=values_.find(k);return it==values_.end()?d:it->second;}
    std::uint32_t u32(const std::string&k,std::uint32_t d)const{return static_cast<std::uint32_t>(std::stoul(get(k,std::to_string(d))));}
    std::uint64_t u64(const std::string&k,std::uint64_t d)const{return static_cast<std::uint64_t>(std::stoull(get(k,std::to_string(d))));}
    double f64(const std::string&k,double d)const{return std::stod(get(k,std::to_string(d)));}
    bool flag(const std::string&k,bool d=false)const{if(!has(k))return d; const auto v=get(k); return v=="true"||v=="1"||v=="yes"||v=="on";}
private:
    std::unordered_map<std::string,std::string> values_;
};
}
