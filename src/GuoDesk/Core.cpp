#include "pch.h"
#include "Core.h"
#include "I18n.h"
#include <shlwapi.h>
#include <cmath>
using namespace winrt;
using namespace Windows::Data::Json;
namespace guodesk {
std::wstring NewId(){ GUID id{}; check_hresult(CoCreateGuid(&id)); wchar_t text[40]{}; StringFromGUID2(id,text,40); return text; }
std::wstring PathKey(std::wstring const& value){ auto p=std::filesystem::absolute(value).lexically_normal().wstring(); std::replace(p.begin(),p.end(),L'/',L'\\'); CharLowerBuffW(p.data(),static_cast<DWORD>(p.size())); while(p.size()>3 && p.back()==L'\\')p.pop_back(); return p; }
bool AddEntry(Zone& z,std::wstring const& path){if(path.empty())return false; auto key=PathKey(path); for(auto const& e:z.entries)if(PathKey(e.path)==key)return false; z.entries.push_back({NewId(),std::filesystem::absolute(path).lexically_normal().wstring()}); return true;}
void SyncMapped(Zone& z){if(z.mappedFolder.empty())return; std::error_code ec;std::filesystem::directory_iterator it(std::filesystem::path(z.mappedFolder),ec);if(ec)return;struct Item{std::wstring name,path;bool dir;};std::vector<Item> items;for(auto const& e:it){std::error_code de;bool dir=e.is_directory(de);items.push_back({e.path().filename().wstring(),e.path().wstring(),dir});}std::sort(items.begin(),items.end(),[](Item const& a,Item const& b){if(a.dir!=b.dir)return a.dir>b.dir;return StrCmpLogicalW(a.name.c_str(),b.name.c_str())<0;});z.entries.clear();for(auto const& i:items)z.entries.push_back({NewId(),i.path});}
void Clamp(Zone& z,RECT const& a){int w=std::max(1L,a.right-a.left),h=std::max(1L,a.bottom-a.top); z.width=std::clamp(z.width,std::min(280,w),w); z.height=std::clamp(z.height,std::min(160,h),h); z.x=std::clamp(z.x,static_cast<int>(a.left),static_cast<int>(a.right)-z.width); z.y=std::clamp(z.y,static_cast<int>(a.top),static_cast<int>(a.bottom)-(z.collapsed?std::min(64,h):z.height));}
static std::wstring Lower(std::wstring v){CharLowerBuffW(v.data(),static_cast<DWORD>(v.size()));return v;}
static std::wstring ExtOf(std::wstring const& path){auto p=std::filesystem::path(path).extension().wstring();if(!p.empty()&&p.front()==L'.')p.erase(p.begin());return Lower(p);}
void DefaultRules(Layout& l){if(!l.rules.empty())return;auto push=[&](std::wstring name,std::vector<std::wstring> exts){l.rules.push_back({NewId(),std::move(name),std::move(exts),{},L""});};push(i18n::Tr(L"文档"),{L"doc",L"docx",L"pdf",L"txt",L"ppt",L"pptx",L"xls",L"xlsx",L"md",L"csv"});push(i18n::Tr(L"图片"),{L"png",L"jpg",L"jpeg",L"gif",L"bmp",L"webp"});push(i18n::Tr(L"安装包"),{L"exe",L"msi",L"zip",L"rar",L"7z"});}
std::vector<std::wstring> ListLooseFiles(std::wstring const& folder){std::vector<std::wstring> out;std::error_code ec;std::filesystem::directory_iterator it(std::filesystem::path(folder),ec);if(ec)return out;for(auto const& e:it){std::error_code de;if(e.is_directory(de))continue;auto path=e.path().wstring();DWORD attr=GetFileAttributesW(path.c_str());if(attr==INVALID_FILE_ATTRIBUTES)continue;if(attr&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM))continue;if(_wcsicmp(e.path().filename().c_str(),L"desktop.ini")==0)continue;out.push_back(path);}std::sort(out.begin(),out.end(),[](auto const& a,auto const& b){return StrCmpLogicalW(a.c_str(),b.c_str())<0;});return out;}
std::vector<std::wstring> DesktopFileList(){std::vector<std::wstring> out;PWSTR p{};if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop,0,nullptr,&p))){out=ListLooseFiles(p);CoTaskMemFree(p);}if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_PublicDesktop,0,nullptr,&p))){auto extra=ListLooseFiles(p);for(auto const& f:extra)if(std::find(out.begin(),out.end(),f)==out.end())out.push_back(f);CoTaskMemFree(p);}return out;}
std::vector<PlanItem> BuildPlan(std::vector<Rule> const& rules,std::vector<Zone> const& zones,std::vector<std::wstring> const& files,std::vector<std::wstring>* unmatched){std::vector<PlanItem> plan;if(unmatched)unmatched->clear();std::vector<Rule> norm;for(auto const& r:rules){if(r.targetZone.empty()||r.exts.empty()&&r.keywords.empty())continue;if(std::find_if(zones.begin(),zones.end(),[&](auto const& z){return z.id==r.targetZone;})==zones.end())continue;Rule n=r;for(auto& e:n.exts)e=Lower(e);for(auto& k:n.keywords)k=Lower(k);norm.push_back(std::move(n));}
for(auto const& f:files){auto ext=ExtOf(f);auto fname=Lower(std::filesystem::path(f).filename().wstring());bool hit=false;for(auto const& r:norm){bool m=false;for(auto const& e:r.exts)if(ext==e){m=true;break;}if(!m)for(auto const& k:r.keywords)if(fname.find(k)!=std::wstring::npos){m=true;break;}if(m){plan.push_back({f,r.name,r.targetZone});hit=true;break;}}if(!hit&&unmatched)unmatched->push_back(f);}return plan;}
int ApplyPlan(Layout& l,std::vector<PlanItem> const& plan){int added=0;for(auto const& p:plan){auto it=std::find_if(l.zones.begin(),l.zones.end(),[&](auto const& z){return z.id==p.zone;});if(it==l.zones.end())continue;if(AddEntry(*it,p.path))++added;}return added;}
TodoItem* AddTodo(Widgets& w,std::wstring const& text){auto t=text;size_t first=t.find_first_not_of(L" \t\r\n");if(first==std::wstring::npos)return nullptr;size_t last=t.find_last_not_of(L" \t\r\n");t=t.substr(first,last-first+1);if(t.size()>2000)t=t.substr(0,2000);w.todos.push_back({NewId(),std::move(t),false});return &w.todos.back();}
void ToggleTodo(Widgets& w,std::wstring const& id){for(auto& t:w.todos)if(t.id==id)t.done=!t.done;}
void RemoveTodo(Widgets& w,std::wstring const& id){std::erase_if(w.todos,[&](auto const& t){return t.id==id;});}
static std::wstring Pad(unsigned value,int width){auto s=std::to_wstring(value);while(s.size()<static_cast<size_t>(width))s.insert(s.begin(),L'0');return s;}
SortKey SortKeyFromString(std::wstring const& value){auto v=Lower(value);if(v==L"type")return SortKey::Type;if(v==L"date")return SortKey::Date;if(v==L"size")return SortKey::Size;return SortKey::Name;}
bool ParseHotkey(std::wstring const& text,Hotkey& out){
 out=Hotkey{};
 auto t=text;size_t a=t.find_first_not_of(L" \t\r\n");
 if(a==std::wstring::npos)return true;
 size_t b=t.find_last_not_of(L" \t\r\n");t=Lower(t.substr(a,b-a+1));
 std::vector<std::wstring> parts;size_t start=0;
 for(;;){auto plus=t.find(L'+',start);if(plus==std::wstring::npos){parts.push_back(t.substr(start));break;}parts.push_back(t.substr(start,plus-start));start=plus+1;}
 for(auto& p:parts){auto pa=p.find_first_not_of(L" \t");if(pa==std::wstring::npos)return false;auto pb=p.find_last_not_of(L" \t");p=p.substr(pa,pb-pa+1);}
 auto isMod=[](std::wstring const& p){return p==L"ctrl"||p==L"control"||p==L"shift"||p==L"alt"||p==L"win"||p==L"windows";};
 if(isMod(parts.back()))return false;
 if(parts.size()<2)return false;
 unsigned mods=0;
 for(size_t i=0;i+1<parts.size();++i){
  auto const& p=parts[i];
  if(p==L"ctrl"||p==L"control")mods|=MOD_CONTROL;
  else if(p==L"shift")mods|=MOD_SHIFT;
  else if(p==L"alt")mods|=MOD_ALT;
  else if(p==L"win"||p==L"windows")mods|=MOD_WIN;
  else return false;
 }
 unsigned vk=0;auto const& k=parts.back();
 if(k.size()==1){wchar_t c=k[0];if(c>=L'a'&&c<=L'z')vk=c-L'a'+L'A';else if(c>=L'0'&&c<=L'9')vk=c;}
 else if(k==L"space")vk=VK_SPACE;
 else if(k==L"esc"||k==L"escape")vk=VK_ESCAPE;
 else if(k==L"tab")vk=VK_TAB;
 else if(k.size()>=2&&k[0]==L'f'){int n=_wtoi(k.c_str()+1);if(n>=1&&n<=24)vk=VK_F1+n-1;}
 if(!vk)return false;
 out.mods=mods;out.vk=vk;return true;
}
std::wstring HotkeyToString(Hotkey const& hotkey){
 if(!hotkey.mods||!hotkey.vk)return L"";
 unsigned vk=hotkey.vk;wchar_t main[8]{};
 if(vk>=VK_F1&&vk<=VK_F24)swprintf_s(main,8,L"F%u",vk-VK_F1+1);
 else if(vk==VK_SPACE)wcscpy_s(main,8,L"Space");
 else if(vk==VK_ESCAPE)wcscpy_s(main,8,L"Esc");
 else if(vk==VK_TAB)wcscpy_s(main,8,L"Tab");
 else if((vk>=L'0'&&vk<=L'9')||(vk>=L'A'&&vk<=L'Z'))main[0]=static_cast<wchar_t>(vk);
 else return L"";
 std::wstring s;
 if(hotkey.mods&MOD_CONTROL)s+=L"Ctrl+";
 if(hotkey.mods&MOD_SHIFT)s+=L"Shift+";
 if(hotkey.mods&MOD_ALT)s+=L"Alt+";
 if(hotkey.mods&MOD_WIN)s+=L"Win+";
 return s+main;
}
void SortEntries(Zone& z,SortKey key,bool descending){
 struct Info{bool ok=false,dir=false;std::wstring name,ext;unsigned long long size=0,ftime=0;};
 std::vector<Info> infos(z.entries.size());
 for(size_t i=0;i<z.entries.size();++i){
  auto& in=infos[i];
  WIN32_FILE_ATTRIBUTE_DATA fa{};
  if(GetFileAttributesExW(z.entries[i].path.c_str(),GetFileExInfoStandard,&fa)){
   in.ok=true;in.dir=(fa.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
   in.size=(static_cast<unsigned long long>(fa.nFileSizeHigh)<<32)|fa.nFileSizeLow;
   in.ftime=(static_cast<unsigned long long>(fa.ftLastWriteTime.dwHighDateTime)<<32)|fa.ftLastWriteTime.dwLowDateTime;
  }
  in.name=Lower(std::filesystem::path(z.entries[i].path).filename().wstring());
  in.ext=ExtOf(z.entries[i].path);
 }
 std::vector<size_t> order(z.entries.size());
 for(size_t i=0;i<order.size();++i)order[i]=i;
 auto natcmp=[](Info const& x,Info const& y){int r=StrCmpLogicalW(x.name.c_str(),y.name.c_str());return r<0?-1:(r>0?1:0);};
 std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){
  auto const& x=infos[a];auto const& y=infos[b];
  if(x.ok!=y.ok)return x.ok;
  if(x.dir!=y.dir)return x.dir>y.dir;
  int cmp=0;
  switch(key){
   case SortKey::Name:cmp=natcmp(x,y);break;
   case SortKey::Type:cmp=x.ext<y.ext?-1:(y.ext<x.ext?1:natcmp(x,y));break;
   case SortKey::Date:cmp=x.ftime<y.ftime?-1:(y.ftime<x.ftime?1:natcmp(x,y));break;
   case SortKey::Size:cmp=x.size<y.size?-1:(y.size<x.size?1:natcmp(x,y));break;
  }
  if(descending)cmp=-cmp;
  return cmp<0;
 });
 std::vector<Entry> out;out.reserve(z.entries.size());
 for(auto idx:order)out.push_back(std::move(z.entries[idx]));
 z.entries=std::move(out);
}
std::string Serialize(Layout const& l){JsonObject root; root.SetNamedValue(L"version",JsonValue::CreateNumberValue(1)); JsonArray zones; for(auto const& z:l.zones){JsonObject j; j.SetNamedValue(L"id",JsonValue::CreateStringValue(z.id)); j.SetNamedValue(L"name",JsonValue::CreateStringValue(z.name)); j.SetNamedValue(L"x",JsonValue::CreateNumberValue(z.x));j.SetNamedValue(L"y",JsonValue::CreateNumberValue(z.y));j.SetNamedValue(L"width",JsonValue::CreateNumberValue(z.width));j.SetNamedValue(L"height",JsonValue::CreateNumberValue(z.height));j.SetNamedValue(L"collapsed",JsonValue::CreateBooleanValue(z.collapsed)); j.SetNamedValue(L"mappedFolder",JsonValue::CreateStringValue(z.mappedFolder)); j.SetNamedValue(L"viewMode",JsonValue::CreateStringValue(z.viewMode)); j.SetNamedValue(L"nameLines",JsonValue::CreateNumberValue(z.nameLines)); j.SetNamedValue(L"sortKey",JsonValue::CreateStringValue(z.sortKey)); j.SetNamedValue(L"sortDescending",JsonValue::CreateBooleanValue(z.sortDescending)); j.SetNamedValue(L"tileSize",JsonValue::CreateNumberValue(z.tileSize)); JsonArray entries;for(auto const& e:z.entries){JsonObject item;item.SetNamedValue(L"id",JsonValue::CreateStringValue(e.id));item.SetNamedValue(L"path",JsonValue::CreateStringValue(e.path));entries.Append(item);}j.SetNamedValue(L"entries",entries);zones.Append(j);}root.SetNamedValue(L"zones",zones);JsonObject s;s.SetNamedValue(L"theme",JsonValue::CreateStringValue(l.settings.theme));s.SetNamedValue(L"compact",JsonValue::CreateBooleanValue(l.settings.compact));s.SetNamedValue(L"language",JsonValue::CreateStringValue(l.settings.language)); s.SetNamedValue(L"hotkey",JsonValue::CreateStringValue(l.settings.hotkey)); s.SetNamedValue(L"snapshots",JsonValue::CreateBooleanValue(l.settings.snapshots));root.SetNamedValue(L"settings",s);JsonArray rs;for(auto const& r:l.rules){JsonObject rj;rj.SetNamedValue(L"id",JsonValue::CreateStringValue(r.id));rj.SetNamedValue(L"name",JsonValue::CreateStringValue(r.name));JsonArray es;for(auto const& e:r.exts)es.Append(JsonValue::CreateStringValue(e));rj.SetNamedValue(L"exts",es);JsonArray ks;for(auto const& k:r.keywords)ks.Append(JsonValue::CreateStringValue(k));rj.SetNamedValue(L"keywords",ks);rj.SetNamedValue(L"zone",JsonValue::CreateStringValue(r.targetZone));rs.Append(rj);}root.SetNamedValue(L"rules",rs);JsonObject wj;wj.SetNamedValue(L"noteVisible",JsonValue::CreateBooleanValue(l.widgets.noteVisible));wj.SetNamedValue(L"todoVisible",JsonValue::CreateBooleanValue(l.widgets.todoVisible));wj.SetNamedValue(L"noteText",JsonValue::CreateStringValue(l.widgets.noteText));wj.SetNamedValue(L"noteX",JsonValue::CreateNumberValue(l.widgets.noteX));wj.SetNamedValue(L"noteY",JsonValue::CreateNumberValue(l.widgets.noteY));wj.SetNamedValue(L"noteW",JsonValue::CreateNumberValue(l.widgets.noteW));wj.SetNamedValue(L"noteH",JsonValue::CreateNumberValue(l.widgets.noteH));wj.SetNamedValue(L"todoX",JsonValue::CreateNumberValue(l.widgets.todoX));wj.SetNamedValue(L"todoY",JsonValue::CreateNumberValue(l.widgets.todoY));wj.SetNamedValue(L"todoW",JsonValue::CreateNumberValue(l.widgets.todoW));wj.SetNamedValue(L"todoH",JsonValue::CreateNumberValue(l.widgets.todoH));JsonArray ts;for(auto const& t:l.widgets.todos){JsonObject tj;tj.SetNamedValue(L"id",JsonValue::CreateStringValue(t.id));tj.SetNamedValue(L"text",JsonValue::CreateStringValue(t.text));tj.SetNamedValue(L"done",JsonValue::CreateBooleanValue(t.done));ts.Append(tj);}wj.SetNamedValue(L"todos",ts);root.SetNamedValue(L"widgets",wj);return to_string(root.Stringify());}
Layout Deserialize(std::string const& text){auto root=JsonObject::Parse(to_hstring(text)); if(root.GetNamedNumber(L"version")!=1)throw std::runtime_error("Unsupported layout version"); Layout l;std::vector<std::wstring> ids;auto number=[](JsonObject const& j,wchar_t const* key){double n=j.GetNamedNumber(key);if(!std::isfinite(n)||n < -100000 || n > 100000)throw std::runtime_error("Invalid geometry");return static_cast<int>(n);};for(auto const& value:root.GetNamedArray(L"zones")){auto j=value.GetObject();Zone z;z.id=j.GetNamedString(L"id");z.name=j.GetNamedString(L"name");if(z.id.empty()||std::find(ids.begin(),ids.end(),z.id)!=ids.end())throw std::runtime_error("Invalid zone identity");ids.push_back(z.id);z.x=number(j,L"x");z.y=number(j,L"y");z.width=number(j,L"width");z.height=number(j,L"height");if(z.width<=0||z.height<=0)throw std::runtime_error("Invalid size");z.collapsed=j.GetNamedBoolean(L"collapsed");if(j.HasKey(L"mappedFolder"))z.mappedFolder=std::wstring(j.GetNamedString(L"mappedFolder"));if(j.HasKey(L"viewMode")){z.viewMode=std::wstring(j.GetNamedString(L"viewMode"));if(z.viewMode!=L"grid"&&z.viewMode!=L"list")z.viewMode=L"grid";}if(j.HasKey(L"nameLines")){double n=j.GetNamedNumber(L"nameLines");z.nameLines=(n>=0&&n<=2&&n==static_cast<int>(n))?static_cast<int>(n):2;}if(j.HasKey(L"sortKey")){z.sortKey=std::wstring(j.GetNamedString(L"sortKey"));if(z.sortKey!=L"name"&&z.sortKey!=L"type"&&z.sortKey!=L"date"&&z.sortKey!=L"size")z.sortKey.clear();}if(j.HasKey(L"sortDescending"))z.sortDescending=j.GetNamedBoolean(L"sortDescending");if(j.HasKey(L"tileSize")){double n=j.GetNamedNumber(L"tileSize");z.tileSize=(n>=0&&n<=2&&n==static_cast<int>(n))?static_cast<int>(n):1;}for(auto const& ev:j.GetNamedArray(L"entries")){auto ej=ev.GetObject();std::wstring p(ej.GetNamedString(L"path")),id(ej.GetNamedString(L"id"));if(p.empty()||!std::filesystem::path(p).is_absolute()||id.empty()||std::find(ids.begin(),ids.end(),id)!=ids.end())throw std::runtime_error("Invalid entry");ids.push_back(id);if(AddEntry(z,p))z.entries.back().id=id;}l.zones.push_back(std::move(z));}if(root.HasKey(L"settings")){auto s=root.GetNamedObject(L"settings");if(s.HasKey(L"theme"))l.settings.theme=std::wstring(s.GetNamedString(L"theme"));if(s.HasKey(L"compact"))l.settings.compact=s.GetNamedBoolean(L"compact");if(s.HasKey(L"language"))l.settings.language=std::wstring(s.GetNamedString(L"language"));if(s.HasKey(L"hotkey")){l.settings.hotkey=std::wstring(s.GetNamedString(L"hotkey"));Hotkey hk;if(!ParseHotkey(l.settings.hotkey,hk))l.settings.hotkey.clear();}if(s.HasKey(L"snapshots"))l.settings.snapshots=s.GetNamedBoolean(L"snapshots");}if(root.HasKey(L"rules")){for(auto const& value:root.GetNamedArray(L"rules")){try{auto rj=value.GetObject();Rule r;r.id=rj.GetNamedString(L"id");r.name=rj.GetNamedString(L"name");if(r.id.empty())continue;if(r.name.empty())r.name=i18n::Tr(L"未命名规则");for(auto const& e:rj.GetNamedArray(L"exts"))r.exts.push_back(std::wstring(e.GetString()));for(auto const& k:rj.GetNamedArray(L"keywords"))r.keywords.push_back(std::wstring(k.GetString()));if(rj.HasKey(L"zone"))r.targetZone=std::wstring(rj.GetNamedString(L"zone"));l.rules.push_back(std::move(r));}catch(...){}}}if(l.rules.empty())DefaultRules(l);if(root.HasKey(L"widgets")){try{auto wj=root.GetNamedObject(L"widgets");auto& w=l.widgets;if(wj.HasKey(L"noteVisible"))w.noteVisible=wj.GetNamedBoolean(L"noteVisible");if(wj.HasKey(L"todoVisible"))w.todoVisible=wj.GetNamedBoolean(L"todoVisible");if(wj.HasKey(L"noteText"))w.noteText=std::wstring(wj.GetNamedString(L"noteText"));auto num=[&](wchar_t const* k,int def)->int{if(!wj.HasKey(k))return def;double n=wj.GetNamedNumber(k);return std::isfinite(n)?static_cast<int>(n):def;};w.noteX=num(L"noteX",w.noteX);w.noteY=num(L"noteY",w.noteY);w.noteW=num(L"noteW",w.noteW);w.noteH=num(L"noteH",w.noteH);w.todoX=num(L"todoX",w.todoX);w.todoY=num(L"todoY",w.todoY);w.todoW=num(L"todoW",w.todoW);w.todoH=num(L"todoH",w.todoH);if(w.noteW<160)w.noteW=160;if(w.noteH<120)w.noteH=120;if(w.todoW<220)w.todoW=220;if(w.todoH<200)w.todoH=200;if(wj.HasKey(L"todos")){for(auto const& value:wj.GetNamedArray(L"todos")){try{auto tj=value.GetObject();TodoItem t;t.id=tj.GetNamedString(L"id");t.text=tj.GetNamedString(L"text");if(t.id.empty()||t.text.empty())continue;t.done=tj.HasKey(L"done")&&tj.GetNamedBoolean(L"done");w.todos.push_back(std::move(t));}catch(...){}}}}catch(...){}}return l;}
Store::Store(std::filesystem::path root):directory(std::move(root)){if(directory.empty()){PWSTR path{};check_hresult(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&path));directory=std::filesystem::path(path)/L"GuoDesk";CoTaskMemFree(path);}std::filesystem::create_directories(directory);}
static std::string Read(std::filesystem::path const& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Cannot read layout");return {std::istreambuf_iterator<char>(f),{}};}
Layout Store::Load(std::wstring& warning){auto main=directory/L"layout.json",backup=directory/L"layout.backup.json";auto trySnapshot=[&](std::wstring& w,Layout& out)->bool{std::error_code ec;std::filesystem::directory_iterator it(directory/L"snapshots",ec);if(ec)return false;std::vector<std::filesystem::path> files;for(auto const& e:it){std::error_code de;if(!e.is_regular_file(de))continue;auto n=e.path().filename().wstring();if(n.starts_with(L"layout-")&&n.ends_with(L".json"))files.push_back(e.path());}std::sort(files.begin(),files.end(),std::greater<std::filesystem::path>());for(auto const& f:files){try{out=Deserialize(Read(f));w+=i18n::Tr(L"已从快照恢复。");return true;}catch(...){}}return false;};if(!std::filesystem::exists(main)){if(std::filesystem::exists(backup)){warning=i18n::Tr(L"主配置缺失，已从备份恢复。");return Deserialize(Read(backup));}Layout snap;if(trySnapshot(warning,snap))return snap;Zone z;z.id=NewId();z.name=i18n::Tr(L"常用");return {{z}};}try{return Deserialize(Read(main));}catch(...){auto preserved=directory/(L"layout.corrupt."+std::to_wstring(GetTickCount64())+L".json");std::filesystem::copy_file(main,preserved);warning=i18n::Tr(L"配置损坏，已保留原文件。");try{auto result=Deserialize(Read(backup));std::filesystem::remove(main);warning+=i18n::Tr(L"已从备份恢复。");return result;}catch(...){std::filesystem::remove(main);Layout snap;if(trySnapshot(warning,snap))return snap;warning+=i18n::Tr(L"使用新的默认分区。");Zone z;z.id=NewId();z.name=i18n::Tr(L"常用");return {{z}};}}}
void Store::Save(Layout const& l){auto main=directory/L"layout.json",temp=directory/L"layout.tmp",backup=directory/L"layout.backup.json";auto data=Serialize(l);HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create layout temporary file");DWORD written{};bool ok=WriteFile(file,data.data(),static_cast<DWORD>(data.size()),&written,nullptr)&&written==data.size()&&FlushFileBuffers(file);CloseHandle(file);if(!ok)throw std::runtime_error("Cannot write layout");if(std::filesystem::exists(main)){if(!ReplaceFileW(main.c_str(),temp.c_str(),backup.c_str(),REPLACEFILE_IGNORE_MERGE_ERRORS,nullptr,nullptr))throw std::runtime_error("Cannot replace layout");}else if(!MoveFileExW(temp.c_str(),main.c_str(),MOVEFILE_WRITE_THROUGH|MOVEFILE_REPLACE_EXISTING))throw std::runtime_error("Cannot save layout");++savesSinceSnapshot;MaybeSnapshot(main);}
void Store::MaybeSnapshot(std::filesystem::path const& main){
 if(!snapshots)return;
 SYSTEMTIME st{};GetLocalTime(&st);
 auto day=Pad(st.wYear,4)+Pad(st.wMonth,2)+Pad(st.wDay,2);
 if(savesSinceSnapshot<20&&snapshotDay==day)return;
 snapshotDay=day;savesSinceSnapshot=0;
 std::error_code ec;auto dir=directory/L"snapshots";std::filesystem::create_directories(dir,ec);if(ec)return;
 auto target=dir/(L"layout-"+day+L"-"+Pad(st.wHour,2)+Pad(st.wMinute,2)+Pad(st.wSecond,2)+L".json");
 std::filesystem::copy_file(main,target,std::filesystem::copy_options::overwrite_existing,ec);if(ec)return;
 std::vector<std::wstring> names;
 for(auto const& e:std::filesystem::directory_iterator(dir,ec)){if(ec)break;std::error_code de;if(!e.is_regular_file(de))continue;auto n=e.path().filename().wstring();if(n.starts_with(L"layout-")&&n.ends_with(L".json"))names.push_back(n);}
 if(names.size()<=5)return;
 std::sort(names.begin(),names.end(),std::greater<std::wstring>());
 for(size_t i=5;i<names.size();++i)std::filesystem::remove(dir/names[i],ec);
}
}

