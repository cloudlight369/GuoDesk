#include "pch.h"
#include "Core.h"
#include "I18n.h"
#include "WebDav.h"
#include <shlwapi.h>
#include <cmath>
using namespace winrt;
using namespace Windows::Data::Json;
namespace guodesk {
std::wstring NewId(){ GUID id{}; check_hresult(CoCreateGuid(&id)); wchar_t text[40]{}; StringFromGUID2(id,text,40); return text; }
std::wstring PathKey(std::wstring const& value){ auto p=std::filesystem::absolute(value).lexically_normal().wstring(); std::replace(p.begin(),p.end(),L'/',L'\\'); CharLowerBuffW(p.data(),static_cast<DWORD>(p.size())); while(p.size()>3 && p.back()==L'\\')p.pop_back(); return p; }
bool AddEntry(Zone& z,std::wstring const& path){if(path.empty())return false; auto key=PathKey(path); for(auto const& e:z.entries)if(PathKey(e.path)==key)return false; z.entries.push_back({NewId(),std::filesystem::absolute(path).lexically_normal().wstring()}); return true;}
std::vector<Entry> ListMapped(std::wstring const& folder){std::vector<Entry> out;if(folder.empty())return out;std::error_code ec;std::filesystem::directory_iterator it(std::filesystem::path(folder),ec);if(ec)return out;struct Item{std::wstring name,path;bool dir;};std::vector<Item> items;for(auto const& e:it){std::error_code de;bool dir=e.is_directory(de);auto path=e.path().wstring();DWORD attr=GetFileAttributesW(path.c_str());if(attr==INVALID_FILE_ATTRIBUTES)continue;if(attr&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM))continue;if(_wcsicmp(e.path().filename().c_str(),L"desktop.ini")==0)continue;items.push_back({e.path().filename().wstring(),path,dir});}std::sort(items.begin(),items.end(),[](Item const& a,Item const& b){if(a.dir!=b.dir)return a.dir>b.dir;return StrCmpLogicalW(a.name.c_str(),b.name.c_str())<0;});for(auto const& i:items)out.push_back({NewId(),i.path});return out;}
void SyncMapped(Zone& z){if(z.mappedFolder.empty())return;std::error_code ec;if(!std::filesystem::exists(z.mappedFolder,ec)||ec)return;z.entries=ListMapped(z.mappedFolder);}
bool UnderRoot(std::wstring const& root,std::wstring const& path){if(root.empty()||path.empty())return false;auto rk=PathKey(root),pk=PathKey(path);if(pk.size()<=rk.size())return false;return pk.starts_with(rk)&&pk[rk.size()]==L'\\';}
std::vector<std::wstring> Crumbs(std::wstring const& root,std::wstring const& current){std::vector<std::wstring> out{root};if(!UnderRoot(root,current))return out;auto rk=PathKey(root),pk=PathKey(current);std::wstring rel=pk.substr(rk.size()+1);std::wstring acc=root;size_t pos=0;while(pos<rel.size()){auto next=rel.find(L'\\',pos);auto part=rel.substr(pos,next==std::wstring::npos?std::wstring::npos:next-pos);acc+=L'\\'+part;out.push_back(acc);if(next==std::wstring::npos)break;pos=next+1;}return out;}
std::wstring CrumbParent(std::wstring const& root,std::wstring const& current){auto chain=Crumbs(root,current);if(chain.size()<2)return L"";if(chain.back()==PathKey(root)||chain.back()==root)return L"";return chain[chain.size()-2];}
void Clamp(Zone& z,RECT const& a){int w=std::max(1L,a.right-a.left),h=std::max(1L,a.bottom-a.top); z.width=std::clamp(z.width,std::min(280,w),w); z.height=std::clamp(z.height,std::min(160,h),h); z.x=std::clamp(z.x,static_cast<int>(a.left),static_cast<int>(a.right)-z.width); z.y=std::clamp(z.y,static_cast<int>(a.top),static_cast<int>(a.bottom)-(z.collapsed?std::min(64,h):z.height));}
static std::wstring Lower(std::wstring v){CharLowerBuffW(v.data(),static_cast<DWORD>(v.size()));return v;}
static std::wstring ExtOf(std::wstring const& path){auto p=std::filesystem::path(path).extension().wstring();if(!p.empty()&&p.front()==L'.')p.erase(p.begin());return Lower(p);}
void DefaultRules(Layout& l){if(!l.rules.empty())return;auto push=[&](std::wstring name,std::vector<std::wstring> exts){l.rules.push_back({NewId(),std::move(name),std::move(exts),{},L""});};push(i18n::Tr(L"文档"),{L"doc",L"docx",L"pdf",L"txt",L"ppt",L"pptx",L"xls",L"xlsx",L"md",L"csv"});push(i18n::Tr(L"图片"),{L"png",L"jpg",L"jpeg",L"gif",L"bmp",L"webp"});push(i18n::Tr(L"安装包"),{L"exe",L"msi",L"zip",L"rar",L"7z"});}
std::vector<std::wstring> ListLooseFiles(std::wstring const& folder){std::vector<std::wstring> out;std::error_code ec;std::filesystem::directory_iterator it(std::filesystem::path(folder),ec);if(ec)return out;for(auto const& e:it){std::error_code de;if(e.is_directory(de))continue;auto path=e.path().wstring();DWORD attr=GetFileAttributesW(path.c_str());if(attr==INVALID_FILE_ATTRIBUTES)continue;if(attr&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM))continue;if(_wcsicmp(e.path().filename().c_str(),L"desktop.ini")==0)continue;out.push_back(path);}std::sort(out.begin(),out.end(),[](auto const& a,auto const& b){return StrCmpLogicalW(a.c_str(),b.c_str())<0;});return out;}
std::vector<std::wstring> DesktopFileList(){std::vector<std::wstring> out;PWSTR p{};if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop,0,nullptr,&p))){out=ListLooseFiles(p);CoTaskMemFree(p);}if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_PublicDesktop,0,nullptr,&p))){auto extra=ListLooseFiles(p);for(auto const& f:extra)if(std::find(out.begin(),out.end(),f)==out.end())out.push_back(f);CoTaskMemFree(p);}return out;}
std::vector<PlanItem> BuildPlan(std::vector<Rule> const& rules,std::vector<Zone> const& zones,std::vector<std::wstring> const& files,std::vector<std::wstring>* unmatched){std::vector<PlanItem> plan;if(unmatched)unmatched->clear();std::vector<Rule> norm;for(auto const& r:rules){if(r.targetZone.empty()||r.exts.empty()&&r.keywords.empty())continue;if(std::find_if(zones.begin(),zones.end(),[&](auto const& z){return z.id==r.targetZone;})==zones.end())continue;Rule n=r;for(auto& e:n.exts)e=Lower(e);for(auto& k:n.keywords)k=Lower(k);norm.push_back(std::move(n));}
for(auto const& f:files){auto ext=ExtOf(f);auto fname=Lower(std::filesystem::path(f).filename().wstring());bool hit=false;for(auto const& r:norm){bool m=false;for(auto const& e:r.exts)if(ext==e){m=true;break;}if(!m)for(auto const& k:r.keywords)if(fname.find(k)!=std::wstring::npos){m=true;break;}if(m){plan.push_back({f,r.name,r.targetZone});hit=true;break;}}if(!hit&&unmatched)unmatched->push_back(f);}return plan;}
int ApplyPlan(Layout& l,std::vector<PlanItem> const& plan){int added=0;for(auto const& p:plan){auto it=std::find_if(l.zones.begin(),l.zones.end(),[&](auto const& z){return z.id==p.zone;});if(it==l.zones.end())continue;if(AddEntry(*it,p.path))++added;}return added;}
std::wstring KnownFolder(std::wstring const& tag){
 GUID id{};
 if(tag==L"downloads")id=FOLDERID_Downloads;else if(tag==L"documents")id=FOLDERID_Documents;else if(tag==L"pictures")id=FOLDERID_Pictures;else if(tag==L"music")id=FOLDERID_Music;else if(tag==L"videos")id=FOLDERID_Videos;else return L"";
 PWSTR p{};if(FAILED(SHGetKnownFolderPath(id,0,nullptr,&p)))return L"";std::wstring out=p;CoTaskMemFree(p);return out;}
std::wstring KnownFolderName(std::wstring const& tag){
 if(tag==L"downloads")return i18n::Tr(L"下载");if(tag==L"documents")return i18n::Tr(L"文档");if(tag==L"pictures")return i18n::Tr(L"图片");if(tag==L"music")return i18n::Tr(L"音乐");if(tag==L"videos")return i18n::Tr(L"视频");return i18n::Tr(L"常用");}
std::vector<ZoneTemplate> BuiltInTemplates(){
 return {
  {L"office",i18n::Tr(L"办公模板"),{{L"文档",L"documents",0,0,494,494},{L"下载",L"downloads",0,506,494,494},{L"图片",L"pictures",506,0,494,494}}},
  {L"media",i18n::Tr(L"影音模板"),{{L"视频",L"videos",0,0,610,1000},{L"音乐",L"music",618,0,382,494},{L"图片",L"pictures",618,512,382,488}}},
  {L"minimal",i18n::Tr(L"极简模板"),{{L"下载",L"downloads",0,0,494,1000},{L"文档",L"documents",506,0,494,1000}}},
 };}
static Zone MakeTemplateZone(std::wstring const& name,std::wstring const& folder,RECT const& work,TemplateZone const& t){
 Zone z;z.id=NewId();z.name=name;z.mappedFolder=folder;
 long long w=std::max(1L,work.right-work.left),h=std::max(1L,work.bottom-work.top);
 z.x=static_cast<int>(work.left+w*t.rx/1000);z.y=static_cast<int>(work.top+h*t.ry/1000);
 z.width=static_cast<int>(std::max<long long>(200,w*t.rw/1000));z.height=static_cast<int>(std::max<long long>(140,h*t.rh/1000));
 return z;}
static bool MapsFolder(Zone const& z,std::wstring const& key){return !z.mappedFolder.empty()&&PathKey(z.mappedFolder)==key;}
int ApplyTemplate(Layout& l,ZoneTemplate const& t,RECT const& work){
 int added=0;
 for(auto const& tz:t.zones){
  auto folder=KnownFolder(tz.folderTag);if(folder.empty())continue;
  auto key=PathKey(folder);
  if(std::any_of(l.zones.begin(),l.zones.end(),[&](auto const& z){return MapsFolder(z,key);}))continue;
  l.zones.push_back(MakeTemplateZone(i18n::Tr(tz.name),folder,work,tz));
  SyncMapped(l.zones.back());++added;}
 return added;}
int AddQuickZone(Layout& l,std::wstring const& tag,RECT const& work){
 auto folder=KnownFolder(tag);if(folder.empty())return 0;
 auto key=PathKey(folder);
 if(std::any_of(l.zones.begin(),l.zones.end(),[&](auto const& z){return MapsFolder(z,key);}))return 0;
 TemplateZone t{KnownFolderName(tag),tag,0,0,494,494};
 l.zones.push_back(MakeTemplateZone(KnownFolderName(tag),folder,work,t));
 auto& z=l.zones.back();int off=30*(static_cast<int>(l.zones.size())-1)%180;z.x+=off;z.y+=off;
 SyncMapped(z);return 1;}
TodoItem* AddTodo(Widgets& w,std::wstring const& text){auto t=text;size_t first=t.find_first_not_of(L" \t\r\n");if(first==std::wstring::npos)return nullptr;size_t last=t.find_last_not_of(L" \t\r\n");t=t.substr(first,last-first+1);if(t.size()>2000)t=t.substr(0,2000);w.todos.push_back({NewId(),std::move(t),false});return &w.todos.back();}
void ToggleTodo(Widgets& w,std::wstring const& id){for(auto& t:w.todos)if(t.id==id)t.done=!t.done;}
void RemoveTodo(Widgets& w,std::wstring const& id){std::erase_if(w.todos,[&](auto const& t){return t.id==id;});}
std::wstring NewStackName(Zone const& z){auto base=i18n::Tr(L"叠放");for(int n=1;;++n){auto name=base+L" "+std::to_wstring(n);bool used=false;for(auto const& s:z.stacks)if(s.name==name){used=true;break;}if(!used)return name;}}
std::wstring CreateStack(Zone& z){Stack s{NewId(),NewStackName(z)};z.stacks.push_back(std::move(s));return z.stacks.back().id;}
void AssignStack(Zone& z,std::wstring const& entryId,std::wstring const& stackId){for(auto& e:z.entries)if(e.id==entryId)e.stack=stackId;}
int StackCount(Zone const& z,std::wstring const& stackId){int n=0;for(auto const& e:z.entries)if(e.stack==stackId)++n;return n;}
void DissolveStack(Zone& z,std::wstring const& stackId){for(auto& e:z.entries)if(e.stack==stackId)e.stack.clear();std::erase_if(z.stacks,[&](Stack const& s){return s.id==stackId;});}
int MoveStack(Layout& l,std::wstring const& stackId,std::wstring const& fromZone,std::wstring const& toZone){
 auto from=std::find_if(l.zones.begin(),l.zones.end(),[&](auto const& z){return z.id==fromZone;});
 auto to=std::find_if(l.zones.begin(),l.zones.end(),[&](auto const& z){return z.id==toZone;});
 if(from==l.zones.end()||to==l.zones.end()||from==to)return 0;
 int moved=0;std::vector<Entry> kept;kept.reserve(from->entries.size());
 for(auto& e:from->entries){
  if(e.stack!=stackId){kept.push_back(std::move(e));continue;}
  auto key=PathKey(e.path);bool dup=false;
  for(auto const& x:to->entries)if(PathKey(x.path)==key){dup=true;break;}
  if(dup)continue;
  to->entries.push_back(std::move(e));++moved;
 }
 from->entries=std::move(kept);
 return moved;
}
std::vector<std::wstring> GroupMemberIds(Layout const& l,std::wstring const& group){std::vector<std::wstring> out;if(group.empty())return out;for(auto const& z:l.zones)if(z.group==group)out.push_back(z.id);return out;}
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
std::vector<MonitorArea> EnumMonitorAreas(){
 std::vector<MonitorArea> out;
 EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR h,HDC,LPRECT,LPARAM p)->BOOL{
  auto* v=reinterpret_cast<std::vector<MonitorArea>*>(p);
  MONITORINFOEXW mi{};mi.cbSize=sizeof(mi);
  if(GetMonitorInfoW(h,&mi))v->push_back({mi.szDevice,mi.rcWork});
  return TRUE;},reinterpret_cast<LPARAM>(&out));
 return out;
}
void Reanchor(RECT& r,std::wstring& mon,int& mx,int& my){
 auto areas=EnumMonitorAreas();
 if(areas.empty())return;
 for(auto const& a:areas){RECT x{};if(IntersectRect(&x,&r,&a.work)){mon=a.device;mx=static_cast<int>(r.left-a.work.left);my=static_cast<int>(r.top-a.work.top);return;}}
 auto it=std::find_if(areas.begin(),areas.end(),[&](auto const& a){return a.device==mon;});
 auto const& target=it!=areas.end()?*it:*std::min_element(areas.begin(),areas.end(),[&](auto const& a,auto const& b){
  auto cx=(r.left+r.right)/2,cy=(r.top+r.bottom)/2;
  auto dist=[&](RECT const& w){long long dx=0,dy=0;if(cx<w.left)dx=w.left-cx;else if(cx>w.right)dx=cx-w.right;if(cy<w.top)dy=w.top-cy;else if(cy>w.bottom)dy=cy-w.bottom;return dx*dx+dy*dy;};
  return dist(a.work)<dist(b.work);});
 int w=r.right-r.left,h=r.bottom-r.top;
 if(it!=areas.end()){r.left+=it->work.left-mx;r.top+=it->work.top-my;r.right=r.left+w;r.bottom=r.top+h;}
 int nl=std::clamp(static_cast<int>(r.left),static_cast<int>(target.work.left),std::max(static_cast<int>(target.work.right)-w,static_cast<int>(target.work.left)));
 int nt=std::clamp(static_cast<int>(r.top),static_cast<int>(target.work.top),std::max(static_cast<int>(target.work.bottom)-h,static_cast<int>(target.work.top)));
 r.left=nl;r.top=nt;r.right=nl+w;r.bottom=nt+h;
 mon=target.device;mx=target.work.left;my=target.work.top;
}
static long long StampFromST(SYSTEMTIME const& st){return static_cast<long long>(st.wYear)*10000+st.wMonth*100+st.wDay;}
static long long StampToFT(long long stamp){SYSTEMTIME st{};st.wYear=static_cast<WORD>(stamp/10000);st.wMonth=static_cast<WORD>(stamp/100%100);st.wDay=static_cast<WORD>(stamp%100);FILETIME f{};SystemTimeToFileTime(&st,&f);return (static_cast<long long>(f.dwHighDateTime)<<32)|f.dwLowDateTime;}
long long DueFromOffset(int days){
 SYSTEMTIME st{};GetLocalTime(&st);
 FILETIME f{};SystemTimeToFileTime(&st,&f);
 long long t=((static_cast<long long>(f.dwHighDateTime)<<32)|f.dwLowDateTime)+static_cast<long long>(days)*864000000000LL;
 f.dwLowDateTime=static_cast<DWORD>(t&0xFFFFFFFF);f.dwHighDateTime=static_cast<DWORD>(t>>32);
 SYSTEMTIME out{};FileTimeToSystemTime(&f,&out);
 return StampFromST(out);
}
bool DueReached(long long due){return due>0&&due<=DueFromOffset(0);}
double ScaledFont(int textSize,double base){double f=textSize<=0?0.85:textSize>=2?1.2:1.0;return base*f;}
int ZoneColorCount(){return 8;}
unsigned ZoneColorRGB(int color){static const unsigned pal[8]={0xE81123,0xF7630C,0xFFB900,0x13A10E,0x00B7C3,0x0078D4,0x8764B8,0xE3008C};return color>=1&&color<=8?pal[color-1]:0u;}
bool IsImagePath(std::wstring const& path){auto f=path.find_last_of(L'.');if(f==std::wstring::npos||f+1>=path.size())return false;auto ext=path.substr(f+1);if(ext.size()>5||ext.find_first_of(L"\\/")!=std::wstring::npos)return false;for(auto& c:ext)c=towlower(c);return ext==L"png"||ext==L"jpg"||ext==L"jpeg"||ext==L"bmp"||ext==L"gif"||ext==L"webp"||ext==L"tif"||ext==L"tiff";}
UndoStack::UndoStack(size_t limit):limit(limit?limit:1){}
void UndoStack::Push(std::wstring label,std::string snapshot){if(snapshot.empty())return;frames.push_back({std::move(label),std::move(snapshot)});while(frames.size()>limit)frames.erase(frames.begin());}
bool UndoStack::Pop(UndoFrame& out){if(frames.empty())return false;out=std::move(frames.back());frames.pop_back();return true;}
bool UndoStack::Empty() const{return frames.empty();}
size_t UndoStack::Count() const{return frames.size();}
size_t UndoStack::Limit() const{return limit;}
std::wstring const& UndoStack::TopLabel() const{static std::wstring const none;return frames.empty()?none:frames.back().label;}
void UndoStack::Clear(){frames.clear();}
std::wstring DueText(long long due){
 if(!due)return L"";
 long long today=DueFromOffset(0);
 long long days=(StampToFT(due)-StampToFT(today))/864000000000LL;
 if(days==0)return i18n::Tr(L"今天");
 if(days==1)return i18n::Tr(L"明天");
 int m=static_cast<int>(due/100%100),d=static_cast<int>(due%100);
 return i18n::TrF(L"{0}月{1}日",{std::to_wstring(m),std::to_wstring(d)});
}
void SetTodoDue(Widgets& w,std::wstring const& id,long long due){for(auto& t:w.todos)if(t.id==id&&t.due!=due){t.due=due;t.reminded=false;}}
NotePage* ActiveNote(Widgets& w){if(w.pages.empty())w.pages.push_back({w.noteText,0});if(w.notePage<0||w.notePage>=static_cast<int>(w.pages.size()))w.notePage=0;return &w.pages[w.notePage];}
void AddNotePage(Widgets& w){if(w.pages.size()>=50)return;ActiveNote(w);w.pages.push_back({L"",0});w.notePage=static_cast<int>(w.pages.size())-1;}
void RemoveNotePage(Widgets& w){if(w.pages.size()<=1){w.pages.clear();w.pages.push_back({L"",0});w.notePage=0;return;}ActiveNote(w);w.pages.erase(w.pages.begin()+w.notePage);if(w.notePage>=static_cast<int>(w.pages.size()))w.notePage=static_cast<int>(w.pages.size())-1;}
bool AppendNote(Widgets& w,std::wstring const& text){auto t=text;size_t first=t.find_first_not_of(L" \t\r\n");if(first==std::wstring::npos)return false;size_t last=t.find_last_not_of(L" \t\r\n");t=t.substr(first,last-first+1);if(t.size()>4000)t=t.substr(0,4000);size_t pos=0;while((pos=t.find(L"\r\n",pos))!=std::wstring::npos){t.replace(pos,2,L"\n");pos+=1;}while((pos=t.find(L'\r'))!=std::wstring::npos)t.replace(pos,1,L"\n");auto* p=ActiveNote(w);if(p->text.empty())p->text=std::move(t);else p->text+=L"\n"+std::move(t);return true;}
bool IsMusicFile(std::wstring const& path){auto ext=std::filesystem::path(path).extension().wstring();if(ext.size()<2)return false;std::wstring e=ext.substr(1);CharLowerBuffW(e.data(),static_cast<DWORD>(e.size()));return e==L"mp3"||e==L"wav"||e==L"flac"||e==L"m4a"||e==L"wma"||e==L"ogg"||e==L"aac";}
std::vector<std::wstring> MusicPlaylist(std::wstring const& folder){
 std::vector<std::wstring> out;std::error_code ec;std::filesystem::directory_iterator it(folder,ec);if(ec)return out;
 for(auto const& e:it){std::error_code de;if(!e.is_regular_file(de))continue;auto p=e.path().wstring();if(IsMusicFile(p))out.push_back(std::move(p));}
 std::sort(out.begin(),out.end(),[](auto const& a,auto const& b){return StrCmpLogicalW(std::filesystem::path(a).filename().c_str(),std::filesystem::path(b).filename().c_str())<0;});
 return out;
}
bool SearchMatch(std::wstring const& text,std::wstring const& query){if(query.empty())return false;auto h=Lower(text);auto q=Lower(query);return h.find(q)!=std::wstring::npos;}
void SearchZones(Layout const& l,std::wstring const& query,std::vector<SearchHit>& out){
 out.clear();if(query.empty())return;
 for(auto const& z:l.zones)for(auto const& e:z.entries){
  auto fname=std::filesystem::path(e.path).filename().wstring();
  if(!SearchMatch(e.path,query)&&!SearchMatch(fname,query))continue;
  out.push_back({e.path,fname,z.name,L""});
  if(out.size()>=100)return;
 }
}
static std::wstring FirstLine(std::wstring const& text){
 size_t start=text.find_first_not_of(L" \t\r\n");
 if(start==std::wstring::npos)return{};
 size_t end=text.find(L'\n',start);
 auto line=end==std::wstring::npos?text.substr(start):text.substr(start,end-start);
 if(line.size()>80)line=line.substr(0,80);
 return line;
}
void SearchWidgets(Layout const& l,std::wstring const& query,std::vector<SearchHit>& out){
 if(query.empty())return;
 for(auto const& t:l.widgets.todos){
  if(t.done||!SearchMatch(t.text,query))continue;
  out.push_back({L"",t.text,L"",L"todo"});
  if(out.size()>=30)return;
 }
 for(auto const& p:l.widgets.pages){
  auto line=FirstLine(p.text);
  if(line.empty()||!SearchMatch(p.text,query))continue;
  out.push_back({L"",line,L"note",L"note"});
  if(out.size()>=30)return;
 }
}
std::wstring WmoText(int code){
 struct E{int code;wchar_t const* text;};
 static const E table[]{{0,L"晴"},{1,L"大部晴朗"},{2,L"局部多云"},{3,L"阴"},{45,L"雾"},{48,L"雾凇"},{51,L"轻毛毛雨"},{53,L"毛毛雨"},{55,L"浓毛毛雨"},{56,L"冻毛毛雨"},{57,L"强冻毛毛雨"},{61,L"小雨"},{63,L"中雨"},{65,L"大雨"},{66,L"冻雨"},{67,L"强冻雨"},{71,L"小雪"},{73,L"中雪"},{75,L"大雪"},{77,L"雪粒"},{80,L"小阵雨"},{81,L"阵雨"},{82,L"强阵雨"},{85,L"小阵雪"},{86,L"阵雪"},{95,L"雷暴"},{96,L"雷暴伴冰雹"},{99,L"强雷暴伴冰雹"}};
 for(auto const& e:table)if(e.code==code)return e.text;
 return L"—";
}
std::wstring WmoEmoji(int code){
 if(code==0)return L"☀️";
 if(code==1)return L"🌤️";
 if(code==2)return L"⛅";
 if(code==3)return L"☁️";
 if(code==45||code==48)return L"🌫️";
 if((code>=51&&code<=57)||(code>=80&&code<=82))return L"🌦️";
 if(code>=95)return L"⛈️";
 if(code>=61&&code<=67)return L"🌧️";
 return L"🌨️";
}
WeatherNow ParseWeatherJson(std::wstring const& json){
 WeatherNow w;
 try{
  auto root=JsonObject::Parse(json);
  auto cur=root.GetNamedObject(L"current");
  w.temp=cur.GetNamedNumber(L"temperature_2m");
  w.code=static_cast<int>(cur.GetNamedNumber(L"weather_code"));
  if(cur.HasKey(L"relative_humidity_2m"))w.humidity=static_cast<int>(cur.GetNamedNumber(L"relative_humidity_2m"));
  auto daily=root.GetNamedObject(L"daily");
  auto times=daily.GetNamedArray(L"time");
  auto codes=daily.GetNamedArray(L"weather_code");
  auto his=daily.GetNamedArray(L"temperature_2m_max");
  auto los=daily.GetNamedArray(L"temperature_2m_min");
  if(times.Size()>0&&codes.Size()>0&&his.Size()>0&&los.Size()>0){w.hi=his.GetAt(0).GetNumber();w.lo=los.GetAt(0).GetNumber();}
  auto isoDate=[&](uint32_t i)->long long{
   if(i>=times.Size())return 0;
   auto s=times.GetAt(i).GetString();
   if(s.size()<10)return 0;
   auto num=[&](uint32_t a,uint32_t b)->int{int v=0;for(uint32_t k=a;k<b;++k){auto c=s[k];if(c<L'0'||c>L'9')return -1;v=v*10+(c-L'0');}return v;};
   int y=num(0,4),m=num(5,7),d=num(8,10);
   if(y<=0||m<1||m>12||d<1||d>31)return 0;
   return static_cast<long long>(y)*10000+m*100+d;
  };
  for(uint32_t i=1;i<times.Size()&&w.days.size()<3;++i)if(i<codes.Size()&&i<his.Size()&&i<los.Size())w.days.push_back({static_cast<int>(codes.GetAt(i).GetNumber()),los.GetAt(i).GetNumber(),his.GetAt(i).GetNumber(),isoDate(i)});
  w.valid=true;
 }catch(...){}
 return w;
}
std::vector<GeoPlace> ParseGeoJson(std::wstring const& json){
 std::vector<GeoPlace> out;
 try{auto root=JsonObject::Parse(json);for(auto const& v:root.GetNamedArray(L"results")){try{auto o=v.GetObject();GeoPlace p;p.name=std::wstring(o.GetNamedString(L"name"));if(o.HasKey(L"country"))p.country=std::wstring(o.GetNamedString(L"country"));p.lat=o.GetNamedNumber(L"latitude");p.lon=o.GetNamedNumber(L"longitude");if(!p.name.empty()&&std::isfinite(p.lat)&&std::isfinite(p.lon))out.push_back(std::move(p));}catch(...){}}}catch(...){}
 return out;
}
GeoPlace ParseIpLocJson(std::wstring const& json){
 GeoPlace p;
 try{auto o=JsonObject::Parse(json);if(std::wstring(o.GetNamedString(L"status"))!=L"success")return p;p.name=std::wstring(o.GetNamedString(L"city"));p.lat=o.GetNamedNumber(L"lat");p.lon=o.GetNamedNumber(L"lon");}catch(...){}
 return p;
}
std::string Serialize(Layout const& l){JsonObject root; root.SetNamedValue(L"version",JsonValue::CreateNumberValue(1)); JsonArray zones; for(auto const& z:l.zones){JsonObject j; j.SetNamedValue(L"id",JsonValue::CreateStringValue(z.id)); j.SetNamedValue(L"name",JsonValue::CreateStringValue(z.name)); j.SetNamedValue(L"x",JsonValue::CreateNumberValue(z.x));j.SetNamedValue(L"y",JsonValue::CreateNumberValue(z.y));j.SetNamedValue(L"width",JsonValue::CreateNumberValue(z.width));j.SetNamedValue(L"height",JsonValue::CreateNumberValue(z.height));j.SetNamedValue(L"collapsed",JsonValue::CreateBooleanValue(z.collapsed)); j.SetNamedValue(L"mappedFolder",JsonValue::CreateStringValue(z.mappedFolder)); j.SetNamedValue(L"viewMode",JsonValue::CreateStringValue(z.viewMode)); j.SetNamedValue(L"nameLines",JsonValue::CreateNumberValue(z.nameLines)); j.SetNamedValue(L"sortKey",JsonValue::CreateStringValue(z.sortKey)); j.SetNamedValue(L"sortDescending",JsonValue::CreateBooleanValue(z.sortDescending)); j.SetNamedValue(L"tileSize",JsonValue::CreateNumberValue(z.tileSize)); j.SetNamedValue(L"mon",JsonValue::CreateStringValue(z.mon)); j.SetNamedValue(L"mx",JsonValue::CreateNumberValue(z.mx)); j.SetNamedValue(L"my",JsonValue::CreateNumberValue(z.my)); j.SetNamedValue(L"group",JsonValue::CreateStringValue(z.group)); j.SetNamedValue(L"groupTab",JsonValue::CreateNumberValue(z.groupTab)); j.SetNamedValue(L"capsule",JsonValue::CreateBooleanValue(z.capsule)); j.SetNamedValue(L"browseInPlace",JsonValue::CreateBooleanValue(z.browseInPlace)); j.SetNamedValue(L"browseFolder",JsonValue::CreateStringValue(z.browseFolder)); j.SetNamedValue(L"color",JsonValue::CreateNumberValue(z.color)); j.SetNamedValue(L"background",JsonValue::CreateStringValue(z.background)); j.SetNamedValue(L"dim",JsonValue::CreateNumberValue(z.dim)); JsonArray stacksArr;for(auto const& st:z.stacks){JsonObject sj;sj.SetNamedValue(L"id",JsonValue::CreateStringValue(st.id));sj.SetNamedValue(L"name",JsonValue::CreateStringValue(st.name));stacksArr.Append(sj);}j.SetNamedValue(L"stacks",stacksArr); JsonArray entries;for(auto const& e:z.entries){JsonObject item;item.SetNamedValue(L"id",JsonValue::CreateStringValue(e.id));item.SetNamedValue(L"path",JsonValue::CreateStringValue(e.path));item.SetNamedValue(L"stack",JsonValue::CreateStringValue(e.stack));entries.Append(item);}j.SetNamedValue(L"entries",entries);zones.Append(j);}root.SetNamedValue(L"zones",zones);JsonObject s;s.SetNamedValue(L"theme",JsonValue::CreateStringValue(l.settings.theme));s.SetNamedValue(L"compact",JsonValue::CreateBooleanValue(l.settings.compact));s.SetNamedValue(L"language",JsonValue::CreateStringValue(l.settings.language)); s.SetNamedValue(L"hotkey",JsonValue::CreateStringValue(l.settings.hotkey)); s.SetNamedValue(L"snapshots",JsonValue::CreateBooleanValue(l.settings.snapshots));s.SetNamedValue(L"guideDone",JsonValue::CreateBooleanValue(l.settings.guideDone));s.SetNamedValue(L"hotkeySearch",JsonValue::CreateStringValue(l.settings.hotkeySearch));s.SetNamedValue(L"hotkeyCapture",JsonValue::CreateStringValue(l.settings.hotkeyCapture));s.SetNamedValue(L"performance",JsonValue::CreateBooleanValue(l.settings.performance));s.SetNamedValue(L"syncUrl",JsonValue::CreateStringValue(l.settings.syncUrl));s.SetNamedValue(L"syncUser",JsonValue::CreateStringValue(l.settings.syncUser));s.SetNamedValue(L"syncPass",JsonValue::CreateStringValue(l.settings.syncPass));s.SetNamedValue(L"syncAuto",JsonValue::CreateBooleanValue(l.settings.syncAuto));s.SetNamedValue(L"textSize",JsonValue::CreateNumberValue(l.settings.textSize));s.SetNamedValue(L"clockStyle",JsonValue::CreateStringValue(l.settings.clockStyle));s.SetNamedValue(L"everything",JsonValue::CreateBooleanValue(l.settings.everything));s.SetNamedValue(L"hotkeyUndo",JsonValue::CreateStringValue(l.settings.hotkeyUndo));s.SetNamedValue(L"tabHover",JsonValue::CreateBooleanValue(l.settings.tabHover));root.SetNamedValue(L"settings",s);JsonArray rs;for(auto const& r:l.rules){JsonObject rj;rj.SetNamedValue(L"id",JsonValue::CreateStringValue(r.id));rj.SetNamedValue(L"name",JsonValue::CreateStringValue(r.name));JsonArray es;for(auto const& e:r.exts)es.Append(JsonValue::CreateStringValue(e));rj.SetNamedValue(L"exts",es);JsonArray ks;for(auto const& k:r.keywords)ks.Append(JsonValue::CreateStringValue(k));rj.SetNamedValue(L"keywords",ks);rj.SetNamedValue(L"zone",JsonValue::CreateStringValue(r.targetZone));rs.Append(rj);}root.SetNamedValue(L"rules",rs);JsonObject wj;wj.SetNamedValue(L"noteVisible",JsonValue::CreateBooleanValue(l.widgets.noteVisible));wj.SetNamedValue(L"todoVisible",JsonValue::CreateBooleanValue(l.widgets.todoVisible));wj.SetNamedValue(L"noteText",JsonValue::CreateStringValue(l.widgets.noteText));wj.SetNamedValue(L"noteX",JsonValue::CreateNumberValue(l.widgets.noteX));wj.SetNamedValue(L"noteY",JsonValue::CreateNumberValue(l.widgets.noteY));wj.SetNamedValue(L"noteW",JsonValue::CreateNumberValue(l.widgets.noteW));wj.SetNamedValue(L"noteH",JsonValue::CreateNumberValue(l.widgets.noteH));wj.SetNamedValue(L"todoX",JsonValue::CreateNumberValue(l.widgets.todoX));wj.SetNamedValue(L"todoY",JsonValue::CreateNumberValue(l.widgets.todoY));wj.SetNamedValue(L"todoW",JsonValue::CreateNumberValue(l.widgets.todoW));wj.SetNamedValue(L"todoH",JsonValue::CreateNumberValue(l.widgets.todoH));wj.SetNamedValue(L"noteTop",JsonValue::CreateBooleanValue(l.widgets.noteTop));wj.SetNamedValue(L"notePage",JsonValue::CreateNumberValue(l.widgets.notePage));wj.SetNamedValue(L"clockVisible",JsonValue::CreateBooleanValue(l.widgets.clockVisible));wj.SetNamedValue(L"clockX",JsonValue::CreateNumberValue(l.widgets.clockX));wj.SetNamedValue(L"clockY",JsonValue::CreateNumberValue(l.widgets.clockY));wj.SetNamedValue(L"clockW",JsonValue::CreateNumberValue(l.widgets.clockW));wj.SetNamedValue(L"clockH",JsonValue::CreateNumberValue(l.widgets.clockH));wj.SetNamedValue(L"noteMon",JsonValue::CreateStringValue(l.widgets.noteMon));wj.SetNamedValue(L"todoMon",JsonValue::CreateStringValue(l.widgets.todoMon));wj.SetNamedValue(L"clockMon",JsonValue::CreateStringValue(l.widgets.clockMon));wj.SetNamedValue(L"noteMX",JsonValue::CreateNumberValue(l.widgets.noteMX));wj.SetNamedValue(L"noteMY",JsonValue::CreateNumberValue(l.widgets.noteMY));wj.SetNamedValue(L"todoMX",JsonValue::CreateNumberValue(l.widgets.todoMX));wj.SetNamedValue(L"todoMY",JsonValue::CreateNumberValue(l.widgets.todoMY));wj.SetNamedValue(L"clockMX",JsonValue::CreateNumberValue(l.widgets.clockMX));wj.SetNamedValue(L"clockMY",JsonValue::CreateNumberValue(l.widgets.clockMY));wj.SetNamedValue(L"musicVisible",JsonValue::CreateBooleanValue(l.widgets.musicVisible));wj.SetNamedValue(L"musicX",JsonValue::CreateNumberValue(l.widgets.musicX));wj.SetNamedValue(L"musicY",JsonValue::CreateNumberValue(l.widgets.musicY));wj.SetNamedValue(L"musicW",JsonValue::CreateNumberValue(l.widgets.musicW));wj.SetNamedValue(L"musicH",JsonValue::CreateNumberValue(l.widgets.musicH));wj.SetNamedValue(L"weatherVisible",JsonValue::CreateBooleanValue(l.widgets.weatherVisible));wj.SetNamedValue(L"weatherX",JsonValue::CreateNumberValue(l.widgets.weatherX));wj.SetNamedValue(L"weatherY",JsonValue::CreateNumberValue(l.widgets.weatherY));wj.SetNamedValue(L"weatherW",JsonValue::CreateNumberValue(l.widgets.weatherW));wj.SetNamedValue(L"weatherH",JsonValue::CreateNumberValue(l.widgets.weatherH));wj.SetNamedValue(L"searchX",JsonValue::CreateNumberValue(l.widgets.searchX));wj.SetNamedValue(L"searchY",JsonValue::CreateNumberValue(l.widgets.searchY));wj.SetNamedValue(L"musicFolder",JsonValue::CreateStringValue(l.widgets.musicFolder));wj.SetNamedValue(L"musicIndex",JsonValue::CreateNumberValue(l.widgets.musicIndex));wj.SetNamedValue(L"musicVol",JsonValue::CreateNumberValue(l.widgets.musicVol));wj.SetNamedValue(L"weatherCity",JsonValue::CreateStringValue(l.widgets.weatherCity));wj.SetNamedValue(L"weatherLat",JsonValue::CreateNumberValue(l.widgets.weatherLat));wj.SetNamedValue(L"weatherLon",JsonValue::CreateNumberValue(l.widgets.weatherLon));wj.SetNamedValue(L"musicMon",JsonValue::CreateStringValue(l.widgets.musicMon));wj.SetNamedValue(L"weatherMon",JsonValue::CreateStringValue(l.widgets.weatherMon));wj.SetNamedValue(L"searchMon",JsonValue::CreateStringValue(l.widgets.searchMon));wj.SetNamedValue(L"musicMX",JsonValue::CreateNumberValue(l.widgets.musicMX));wj.SetNamedValue(L"musicMY",JsonValue::CreateNumberValue(l.widgets.musicMY));wj.SetNamedValue(L"weatherMX",JsonValue::CreateNumberValue(l.widgets.weatherMX));wj.SetNamedValue(L"weatherMY",JsonValue::CreateNumberValue(l.widgets.weatherMY));wj.SetNamedValue(L"searchMX",JsonValue::CreateNumberValue(l.widgets.searchMX));wj.SetNamedValue(L"searchMY",JsonValue::CreateNumberValue(l.widgets.searchMY));JsonArray ps;for(auto const& p:l.widgets.pages){JsonObject pj;pj.SetNamedValue(L"text",JsonValue::CreateStringValue(p.text));pj.SetNamedValue(L"color",JsonValue::CreateNumberValue(p.color));ps.Append(pj);}wj.SetNamedValue(L"pages",ps);JsonArray ts;for(auto const& t:l.widgets.todos){JsonObject tj;tj.SetNamedValue(L"id",JsonValue::CreateStringValue(t.id));tj.SetNamedValue(L"text",JsonValue::CreateStringValue(t.text));tj.SetNamedValue(L"done",JsonValue::CreateBooleanValue(t.done));tj.SetNamedValue(L"due",JsonValue::CreateNumberValue(static_cast<double>(t.due)));tj.SetNamedValue(L"reminded",JsonValue::CreateBooleanValue(t.reminded));ts.Append(tj);}wj.SetNamedValue(L"todos",ts);root.SetNamedValue(L"widgets",wj);return to_string(root.Stringify());}
Layout Deserialize(std::string const& text){auto root=JsonObject::Parse(to_hstring(text)); if(root.GetNamedNumber(L"version")!=1)throw std::runtime_error("Unsupported layout version"); Layout l;std::vector<std::wstring> ids;auto number=[](JsonObject const& j,wchar_t const* key){double n=j.GetNamedNumber(key);if(!std::isfinite(n)||n < -100000 || n > 100000)throw std::runtime_error("Invalid geometry");return static_cast<int>(n);};for(auto const& value:root.GetNamedArray(L"zones")){auto j=value.GetObject();Zone z;z.id=j.GetNamedString(L"id");z.name=j.GetNamedString(L"name");if(z.id.empty()||std::find(ids.begin(),ids.end(),z.id)!=ids.end())throw std::runtime_error("Invalid zone identity");ids.push_back(z.id);z.x=number(j,L"x");z.y=number(j,L"y");z.width=number(j,L"width");z.height=number(j,L"height");if(z.width<=0||z.height<=0)throw std::runtime_error("Invalid size");z.collapsed=j.GetNamedBoolean(L"collapsed");if(j.HasKey(L"mappedFolder"))z.mappedFolder=std::wstring(j.GetNamedString(L"mappedFolder"));if(j.HasKey(L"viewMode")){z.viewMode=std::wstring(j.GetNamedString(L"viewMode"));if(z.viewMode!=L"grid"&&z.viewMode!=L"list")z.viewMode=L"grid";}if(j.HasKey(L"nameLines")){double n=j.GetNamedNumber(L"nameLines");z.nameLines=(n>=0&&n<=2&&n==static_cast<int>(n))?static_cast<int>(n):2;}if(j.HasKey(L"sortKey")){z.sortKey=std::wstring(j.GetNamedString(L"sortKey"));if(z.sortKey!=L"name"&&z.sortKey!=L"type"&&z.sortKey!=L"date"&&z.sortKey!=L"size")z.sortKey.clear();}if(j.HasKey(L"sortDescending"))z.sortDescending=j.GetNamedBoolean(L"sortDescending");if(j.HasKey(L"tileSize")){double n=j.GetNamedNumber(L"tileSize");z.tileSize=(n>=0&&n<=2&&n==static_cast<int>(n))?static_cast<int>(n):1;}if(j.HasKey(L"mon"))z.mon=std::wstring(j.GetNamedString(L"mon"));if(j.HasKey(L"mx")){double n=j.GetNamedNumber(L"mx");if(std::isfinite(n))z.mx=static_cast<int>(n);}if(j.HasKey(L"my")){double n=j.GetNamedNumber(L"my");if(std::isfinite(n))z.my=static_cast<int>(n);}if(j.HasKey(L"group"))z.group=std::wstring(j.GetNamedString(L"group"));if(j.HasKey(L"groupTab")){double n=j.GetNamedNumber(L"groupTab");if(std::isfinite(n)&&n>=0&&n==static_cast<int>(n))z.groupTab=static_cast<int>(n);}if(j.HasKey(L"capsule"))z.capsule=j.GetNamedBoolean(L"capsule");if(j.HasKey(L"browseInPlace"))z.browseInPlace=j.GetNamedBoolean(L"browseInPlace");if(j.HasKey(L"browseFolder"))z.browseFolder=std::wstring(j.GetNamedString(L"browseFolder"));if(j.HasKey(L"color")){double n=j.GetNamedNumber(L"color");z.color=(n>=0&&n<=8&&n==static_cast<int>(n))?static_cast<int>(n):0;}if(j.HasKey(L"background"))z.background=std::wstring(j.GetNamedString(L"background"));if(j.HasKey(L"dim")){double n=j.GetNamedNumber(L"dim");z.dim=(n>=0&&n<=2&&n==static_cast<int>(n))?static_cast<int>(n):1;}if(j.HasKey(L"stacks")){for(auto const& sv:j.GetNamedArray(L"stacks")){try{auto sj=sv.GetObject();Stack st;st.id=sj.GetNamedString(L"id");st.name=sj.GetNamedString(L"name");if(!st.id.empty()&&!st.name.empty()&&std::find_if(z.stacks.begin(),z.stacks.end(),[&](Stack const& x){return x.id==st.id;})==z.stacks.end())z.stacks.push_back(std::move(st));}catch(...){}}}for(auto const& ev:j.GetNamedArray(L"entries")){auto ej=ev.GetObject();std::wstring p(ej.GetNamedString(L"path")),id(ej.GetNamedString(L"id"));if(p.empty()||!std::filesystem::path(p).is_absolute()||id.empty()||std::find(ids.begin(),ids.end(),id)!=ids.end())throw std::runtime_error("Invalid entry");ids.push_back(id);if(AddEntry(z,p)){z.entries.back().id=id;if(ej.HasKey(L"stack"))z.entries.back().stack=std::wstring(ej.GetNamedString(L"stack"));}}l.zones.push_back(std::move(z));}if(root.HasKey(L"settings")){auto s=root.GetNamedObject(L"settings");if(s.HasKey(L"theme"))l.settings.theme=std::wstring(s.GetNamedString(L"theme"));if(s.HasKey(L"compact"))l.settings.compact=s.GetNamedBoolean(L"compact");if(s.HasKey(L"language"))l.settings.language=std::wstring(s.GetNamedString(L"language"));if(s.HasKey(L"hotkey")){l.settings.hotkey=std::wstring(s.GetNamedString(L"hotkey"));Hotkey hk;if(!IsDoubleCtrlHotkey(l.settings.hotkey)&&!ParseHotkey(l.settings.hotkey,hk))l.settings.hotkey.clear();}if(s.HasKey(L"snapshots"))l.settings.snapshots=s.GetNamedBoolean(L"snapshots");if(s.HasKey(L"guideDone"))l.settings.guideDone=s.GetNamedBoolean(L"guideDone");if(s.HasKey(L"hotkeySearch")){l.settings.hotkeySearch=std::wstring(s.GetNamedString(L"hotkeySearch"));Hotkey hks;if(!l.settings.hotkeySearch.empty()&&!ParseHotkey(l.settings.hotkeySearch,hks))l.settings.hotkeySearch.clear();}if(s.HasKey(L"hotkeyCapture")){l.settings.hotkeyCapture=std::wstring(s.GetNamedString(L"hotkeyCapture"));Hotkey hkc;if(!l.settings.hotkeyCapture.empty()&&!ParseHotkey(l.settings.hotkeyCapture,hkc))l.settings.hotkeyCapture.clear();}if(s.HasKey(L"performance"))l.settings.performance=s.GetNamedBoolean(L"performance");if(s.HasKey(L"syncUrl")){l.settings.syncUrl=std::wstring(s.GetNamedString(L"syncUrl"));webdav::UrlParts up;if(!l.settings.syncUrl.empty()&&!webdav::ParseUrl(l.settings.syncUrl,up))l.settings.syncUrl.clear();}if(s.HasKey(L"syncUser"))l.settings.syncUser=std::wstring(s.GetNamedString(L"syncUser"));if(s.HasKey(L"syncPass"))l.settings.syncPass=std::wstring(s.GetNamedString(L"syncPass"));if(s.HasKey(L"syncAuto"))l.settings.syncAuto=s.GetNamedBoolean(L"syncAuto");if(s.HasKey(L"textSize")){double n=s.GetNamedNumber(L"textSize");l.settings.textSize=(n>=0&&n<=2&&n==static_cast<int>(n))?static_cast<int>(n):1;}if(s.HasKey(L"clockStyle")){l.settings.clockStyle=std::wstring(s.GetNamedString(L"clockStyle"));if(l.settings.clockStyle!=L"analog")l.settings.clockStyle.clear();}if(s.HasKey(L"everything"))l.settings.everything=s.GetNamedBoolean(L"everything");if(s.HasKey(L"hotkeyUndo")){l.settings.hotkeyUndo=std::wstring(s.GetNamedString(L"hotkeyUndo"));Hotkey hku;if(!l.settings.hotkeyUndo.empty()&&!ParseHotkey(l.settings.hotkeyUndo,hku))l.settings.hotkeyUndo.clear();}if(s.HasKey(L"tabHover"))l.settings.tabHover=s.GetNamedBoolean(L"tabHover");}if(root.HasKey(L"rules")){for(auto const& value:root.GetNamedArray(L"rules")){try{auto rj=value.GetObject();Rule r;r.id=rj.GetNamedString(L"id");r.name=rj.GetNamedString(L"name");if(r.id.empty())continue;if(r.name.empty())r.name=i18n::Tr(L"未命名规则");for(auto const& e:rj.GetNamedArray(L"exts"))r.exts.push_back(std::wstring(e.GetString()));for(auto const& k:rj.GetNamedArray(L"keywords"))r.keywords.push_back(std::wstring(k.GetString()));if(rj.HasKey(L"zone"))r.targetZone=std::wstring(rj.GetNamedString(L"zone"));l.rules.push_back(std::move(r));}catch(...){}}}if(l.rules.empty())DefaultRules(l);if(root.HasKey(L"widgets")){try{auto wj=root.GetNamedObject(L"widgets");auto& w=l.widgets;if(wj.HasKey(L"noteVisible"))w.noteVisible=wj.GetNamedBoolean(L"noteVisible");if(wj.HasKey(L"todoVisible"))w.todoVisible=wj.GetNamedBoolean(L"todoVisible");if(wj.HasKey(L"clockVisible"))w.clockVisible=wj.GetNamedBoolean(L"clockVisible");if(wj.HasKey(L"musicVisible"))w.musicVisible=wj.GetNamedBoolean(L"musicVisible");if(wj.HasKey(L"weatherVisible"))w.weatherVisible=wj.GetNamedBoolean(L"weatherVisible");if(wj.HasKey(L"noteText"))w.noteText=std::wstring(wj.GetNamedString(L"noteText"));auto num=[&](wchar_t const* k,int def)->int{if(!wj.HasKey(k))return def;double n=wj.GetNamedNumber(k);return std::isfinite(n)?static_cast<int>(n):def;};w.noteX=num(L"noteX",w.noteX);w.noteY=num(L"noteY",w.noteY);w.noteW=num(L"noteW",w.noteW);w.noteH=num(L"noteH",w.noteH);w.todoX=num(L"todoX",w.todoX);w.todoY=num(L"todoY",w.todoY);w.todoW=num(L"todoW",w.todoW);w.todoH=num(L"todoH",w.todoH);w.clockX=num(L"clockX",w.clockX);w.clockY=num(L"clockY",w.clockY);w.clockW=num(L"clockW",w.clockW);w.clockH=num(L"clockH",w.clockH);if(w.noteW<160)w.noteW=160;if(w.noteH<120)w.noteH=120;if(w.todoW<220)w.todoW=220;if(w.todoH<200)w.todoH=200;if(w.clockW<150)w.clockW=150;if(w.clockH<110)w.clockH=110;w.musicX=num(L"musicX",w.musicX);w.musicY=num(L"musicY",w.musicY);w.musicW=num(L"musicW",w.musicW);w.musicH=num(L"musicH",w.musicH);w.weatherX=num(L"weatherX",w.weatherX);w.weatherY=num(L"weatherY",w.weatherY);w.weatherW=num(L"weatherW",w.weatherW);w.weatherH=num(L"weatherH",w.weatherH);w.searchX=num(L"searchX",w.searchX);w.searchY=num(L"searchY",w.searchY);if(w.musicW<240)w.musicW=240;if(w.musicH<300)w.musicH=300;if(w.weatherW<220)w.weatherW=220;if(w.weatherH<280)w.weatherH=280;w.musicIndex=num(L"musicIndex",w.musicIndex);if(w.musicIndex<0)w.musicIndex=0;w.musicVol=num(L"musicVol",w.musicVol);w.musicVol=std::clamp(w.musicVol,0,100);if(wj.HasKey(L"noteTop"))w.noteTop=wj.GetNamedBoolean(L"noteTop");if(wj.HasKey(L"notePage")){double n=wj.GetNamedNumber(L"notePage");w.notePage=(std::isfinite(n)&&n>=0)?static_cast<int>(n):0;}auto mon=[&](wchar_t const* k,std::wstring& target){if(wj.HasKey(k))target=std::wstring(wj.GetNamedString(k));};mon(L"noteMon",w.noteMon);mon(L"todoMon",w.todoMon);mon(L"clockMon",w.clockMon);mon(L"musicMon",w.musicMon);mon(L"weatherMon",w.weatherMon);mon(L"searchMon",w.searchMon);auto num2=[&](wchar_t const* k,int& target){if(wj.HasKey(k)){double n=wj.GetNamedNumber(k);if(std::isfinite(n))target=static_cast<int>(n);}};num2(L"noteMX",w.noteMX);num2(L"noteMY",w.noteMY);num2(L"todoMX",w.todoMX);num2(L"todoMY",w.todoMY);num2(L"clockMX",w.clockMX);num2(L"clockMY",w.clockMY);num2(L"musicMX",w.musicMX);num2(L"musicMY",w.musicMY);num2(L"weatherMX",w.weatherMX);num2(L"weatherMY",w.weatherMY);num2(L"searchMX",w.searchMX);num2(L"searchMY",w.searchMY);if(wj.HasKey(L"musicFolder"))w.musicFolder=std::wstring(wj.GetNamedString(L"musicFolder"));if(wj.HasKey(L"weatherCity"))w.weatherCity=std::wstring(wj.GetNamedString(L"weatherCity"));auto dnum=[&](wchar_t const* k,double& target){if(!wj.HasKey(k))return;double n=wj.GetNamedNumber(k);if(std::isfinite(n))target=n;};dnum(L"weatherLat",w.weatherLat);dnum(L"weatherLon",w.weatherLon);if(w.weatherLat<-90||w.weatherLat>90||w.weatherLon<-180||w.weatherLon>180){w.weatherLat=999;w.weatherLon=999;}if(wj.HasKey(L"pages")){for(auto const& value:wj.GetNamedArray(L"pages")){try{auto pj=value.GetObject();NotePage p;p.text=std::wstring(pj.GetNamedString(L"text"));if(pj.HasKey(L"color")){double n=pj.GetNamedNumber(L"color");p.color=(n>=0&&n<=5&&n==static_cast<int>(n))?static_cast<int>(n):0;}w.pages.push_back(std::move(p));}catch(...){}}}if(w.pages.empty()&&!w.noteText.empty())w.pages.push_back({w.noteText,0});if(wj.HasKey(L"todos")){for(auto const& value:wj.GetNamedArray(L"todos")){try{auto tj=value.GetObject();TodoItem t;t.id=tj.GetNamedString(L"id");t.text=tj.GetNamedString(L"text");if(t.id.empty()||t.text.empty())continue;t.done=tj.HasKey(L"done")&&tj.GetNamedBoolean(L"done");if(tj.HasKey(L"due")){double n=tj.GetNamedNumber(L"due");if(std::isfinite(n)&&n>0)t.due=static_cast<long long>(n);}if(tj.HasKey(L"reminded"))t.reminded=tj.GetNamedBoolean(L"reminded");w.todos.push_back(std::move(t));}catch(...){}}}}catch(...){}}for(auto& z:l.zones){if(z.mappedFolder.empty()||!z.browseInPlace||!UnderRoot(z.mappedFolder,z.browseFolder))z.browseFolder.clear();if(!IsImagePath(z.background))z.background.clear();if(z.group.empty())continue;auto members=GroupMemberIds(l,z.group);if(members.size()<2){z.group.clear();z.groupTab=0;continue;}z.groupTab=std::clamp(z.groupTab,0,static_cast<int>(members.size())-1);}for(auto& z:l.zones)for(auto& e:z.entries)if(!e.stack.empty()&&std::find_if(z.stacks.begin(),z.stacks.end(),[&](Stack const& s){return s.id==e.stack;})==z.stacks.end())e.stack.clear();return l;}
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

