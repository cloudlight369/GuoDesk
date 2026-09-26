#pragma once
namespace guodesk {
struct Entry { std::wstring id, path; };
struct Zone { std::wstring id, name=L"常用"; int x=120,y=120,width=440,height=360; bool collapsed=false; std::vector<Entry> entries; };
struct Layout { std::vector<Zone> zones; };
std::wstring NewId();
std::wstring PathKey(std::wstring const& path);
bool AddEntry(Zone& zone,std::wstring const& path);
void Clamp(Zone& zone,RECT const& area);
std::string Serialize(Layout const& layout);
Layout Deserialize(std::string const& json);
class Store {
 std::filesystem::path directory;
public:
 explicit Store(std::filesystem::path root={});
 Layout Load(std::wstring& warning);
 void Save(Layout const& layout);
 std::filesystem::path Directory() const {return directory;}
};
}
