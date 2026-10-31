#include "core/workspace.hpp"
#include <algorithm>
#include <system_error>
namespace CrashEdit {
static std::string kindOf(const std::filesystem::path& p,bool dir) {
 if(dir) return "Folder"; auto e=p.extension().string();
 if(e==".nsf"||e==".NSF") return "NSF — Resource";
 if(e==".nsd"||e==".NSD") return "NSD — Level Details";
 if(e==".iso"||e==".ISO") return "Disc image";
 return e.empty()?"File":e.substr(1)+" file";
}
ResourceNode Workspace::scanDirectory(const std::filesystem::path& p,unsigned depth) {
 ResourceNode n; n.name=p.filename().empty()?p.string():p.filename().string(); n.path=p;
 std::error_code ec; n.isDirectory=std::filesystem::is_directory(p,ec); n.kind=kindOf(p,n.isDirectory);
 if(!n.isDirectory||depth>=4) return n;
 std::vector<std::filesystem::directory_entry> entries;
 for(std::filesystem::directory_iterator i(p,ec),end;!ec&&i!=end;i.increment(ec)) entries.push_back(*i);
 std::sort(entries.begin(),entries.end(),[](const auto&a,const auto&b){auto ad=a.is_directory(),bd=b.is_directory(); if(ad!=bd)return ad>bd;return a.path().filename().string()<b.path().filename().string();});
 for(const auto&e:entries){auto s=e.path().filename().string();if(!s.empty()&&s[0]!='.')n.children.push_back(scanDirectory(e.path(),depth+1));}
 return n;
}
bool Workspace::open(const std::filesystem::path&p,std::string&error){
 std::error_code ec;auto a=std::filesystem::weakly_canonical(p,ec);
 if(ec||!std::filesystem::exists(a,ec)||!std::filesystem::is_directory(a,ec)){error="Select an existing folder to open as a workspace.";return false;}
 rootPath_=a;resources_.clear();selectedResource_.clear();resources_=std::move(scanDirectory(rootPath_,0).children);return true;
}
void Workspace::close(){rootPath_.clear();resources_.clear();selectedResource_.clear();}
}
