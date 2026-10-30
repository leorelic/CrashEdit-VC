#pragma once
#include <filesystem>
#include <string>
#include <utility>
#include <vector>
namespace CrashEdit {
struct ResourceNode { std::string name, kind; std::filesystem::path path; std::vector<ResourceNode> children; bool isDirectory=false; };
class Workspace {
public:
 bool open(const std::filesystem::path&, std::string&); void close();
 bool isOpen() const noexcept { return !rootPath_.empty(); }
 const std::filesystem::path& rootPath() const noexcept { return rootPath_; }
 const std::vector<ResourceNode>& resources() const noexcept { return resources_; }
 const std::string& selectedResource() const noexcept { return selectedResource_; }
 void selectResource(std::string s) { selectedResource_=std::move(s); }
private:
 static ResourceNode scanDirectory(const std::filesystem::path&, unsigned);
 std::filesystem::path rootPath_; std::vector<ResourceNode> resources_; std::string selectedResource_;
};
}
