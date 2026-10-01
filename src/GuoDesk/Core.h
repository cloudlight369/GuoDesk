#pragma once
namespace guodesk {
struct Entry { std::wstring id, path; };
struct Zone { std::wstring id, name=L"常用"; int x=120,y=120,width=440,height=360; bool collapsed=false; std::vector<Entry> entries; std::wstring mappedFolder; std::wstring viewMode=L"grid"; int nameLines=2; std::wstring sortKey; bool sortDescending=false; int tileSize=1; };
struct Settings { std::wstring theme; bool compact=false; std::wstring language; std::wstring hotkey=L"Ctrl+Alt+G"; bool snapshots=true; };
struct Rule { std::wstring id, name; std::vector<std::wstring> exts, keywords; std::wstring targetZone; };
struct TodoItem { std::wstring id, text; bool done=false; };
struct Widgets {
 bool noteVisible=false,todoVisible=false;
 std::wstring noteText;
 int noteX=340,noteY=180,noteW=300,noteH=240;
 int todoX=680,todoY=180,todoW=300,todoH=420;
 std::vector<TodoItem> todos;
};
struct Layout { std::vector<Zone> zones; Settings settings; std::vector<Rule> rules; Widgets widgets; };
struct PlanItem { std::wstring path, rule, zone; };
enum class SortKey { Name, Type, Date, Size };
struct Hotkey { unsigned mods=0, vk=0; };
SortKey SortKeyFromString(std::wstring const& value);
bool ParseHotkey(std::wstring const& text,Hotkey& out);
std::wstring HotkeyToString(Hotkey const& hotkey);
void SortEntries(Zone& zone,SortKey key,bool descending);
std::wstring NewId();
std::wstring PathKey(std::wstring const& path);
bool AddEntry(Zone& zone,std::wstring const& path);
void SyncMapped(Zone& zone);
void Clamp(Zone& zone,RECT const& area);
void DefaultRules(Layout& layout);
std::vector<std::wstring> ListLooseFiles(std::wstring const& folder);
std::vector<std::wstring> DesktopFileList();
std::vector<PlanItem> BuildPlan(std::vector<Rule> const& rules,std::vector<Zone> const& zones,std::vector<std::wstring> const& files,std::vector<std::wstring>* unmatched);
int ApplyPlan(Layout& layout,std::vector<PlanItem> const& plan);
TodoItem* AddTodo(Widgets& widgets,std::wstring const& text);
void ToggleTodo(Widgets& widgets,std::wstring const& id);
void RemoveTodo(Widgets& widgets,std::wstring const& id);
std::string Serialize(Layout const& layout);
Layout Deserialize(std::string const& json);
class Store {
 std::filesystem::path directory;
 bool snapshots=true;
 int savesSinceSnapshot=0;
 std::wstring snapshotDay;
 void MaybeSnapshot(std::filesystem::path const& main);
public:
 explicit Store(std::filesystem::path root={});
 Layout Load(std::wstring& warning);
 void Save(Layout const& layout);
 void SetSnapshots(bool enabled){snapshots=enabled;}
 std::filesystem::path Directory() const {return directory;}
};
}
