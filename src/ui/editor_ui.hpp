#pragma once
#include "core/workspace.hpp"
#include "crash/formats/nsf/nsf_archive.hpp"
#include <string>
namespace CrashEdit {
class EditorUI {
public: explicit EditorUI(Workspace&w):workspace_(w){} void draw();
private:
 void drawMenu();void drawToolbar();void drawHierarchy();void drawViewport();void drawBottom();
 void drawNode(const ResourceNode&);
 Workspace&workspace_;bool openDialog_=false,about_=false,properties_=true,console_=true,assets_=true;
 char path_[1024]{};std::string status_="Ready — open a workspace to inspect Crash resources.";
 Crash::NsfArchive nsf_;bool nsfValid_=false;
};
}
