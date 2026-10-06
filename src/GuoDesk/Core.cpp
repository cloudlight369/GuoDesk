#include "pch.h"
#include "Core.h"
#include "I18n.h"
#include "WebDav.h"
#include <shlwapi.h>
#include <cmath>
#include <cwctype>
using namespace winrt;
using namespace Windows::Data::Json;
namespace guodesk {
std::wstring NewId(){ GUID id{}; check_hresult(CoCreateGuid(&id)); wchar_t text[40]{}; StringFromGUID2(id,text,40); return text; }
std::wstring PathKey(std::wstring const& value){ auto p=std::filesystem::absolute(value).lexically_normal().wstring(); std::replace(p.begin(),p.end(),L'/',L'\\'); CharLowerBuffW(p.data(),static_cast<DWORD>(p.size())); while(p.size()>3 && p.back()==L'\\')p.pop_back(); return p; }
bool AddEntry(Zone& z,std::wstring const& path){if(path.empty())return false; auto key=PathKey(path); for(auto const& e:z.entries)if(PathKey(e.path)==key)return false; z.entries.push_back({NewId(),std::filesystem::absolute(path).lexically_normal().wstring()}); return true;}
std::vector<Entry> ListMapped(std::wstring const& folder){std::vector<Entry> out;if(folder.empty())return out;std::error_code ec;std::filesystem::directory_iterator it(std::filesystem::path(folder),ec);if(ec)return out;struct Item{std::wstring name,path;bool dir;};std::vector<Item> items;for(auto const& e:it){std::error_code de;bool dir=e.is_directory(de);auto path=e.path().wstring();DWORD attr=GetFileAttributesW(path.c_str());if(attr==INVALID_FILE_ATTRIBUTES)continue;if(attr&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM))continue;if(_wcsicmp(e.path().filename().c_str(),L"desktop.ini")==0)continue;items.push_back({e.path().filename().wstring(),path,dir});}std::sort(items.begin(),items.end(),[](Item const& a,Item const& b){if(a.dir!=b.dir)return a.dir>b.dir;return StrCmpLogicalW(a.name.c_str(),b.name.c_str())<0;});for(auto const& i:items)out.push_back({NewId(),i.path});return out;}
void SyncMapped(Zone& z){if(z.mappedFolder.empty())return;std::error_code ec;if(!std::filesystem::exists(z.mappedFolder,ec)||ec)return;z.entries=ListMapped(z.mappedFolder);}
bool UnderRoot(std::wstring const& root,std::wstring const& path){if(root.empty()||path.empty())return false;auto rk=PathKey(root),pk=PathKey(path);if(pk.size()<=rk.size())return false;return pk.starts_with(rk)&&pk[rk.size()]==L'\\';}
// 把文件夹放进它自己或它自己的子目录：SHFileOperationW 只会报错，可能留下半副本，故在纯函数层先判掉
bool SelfNesting(std::wstring const& source,std::wstring const& destDir){if(source.empty()||destDir.empty())return false;return PathKey(source)==PathKey(destDir)||UnderRoot(source,destDir);}
std::vector<std::wstring> Crumbs(std::wstring const& root,std::wstring const& current){std::vector<std::wstring> out{root};if(!UnderRoot(root,current))return out;auto rk=PathKey(root),pk=PathKey(current);std::wstring rel=pk.substr(rk.size()+1);std::wstring acc=root;size_t pos=0;while(pos<rel.size()){auto next=rel.find(L'\\',pos);auto part=rel.substr(pos,next==std::wstring::npos?std::wstring::npos:next-pos);acc+=L'\\'+part;out.push_back(acc);if(next==std::wstring::npos)break;pos=next+1;}return out;}
std::wstring CrumbParent(std::wstring const& root,std::wstring const& current){auto chain=Crumbs(root,current);if(chain.size()<2)return L"";if(chain.back()==PathKey(root)||chain.back()==root)return L"";return chain[chain.size()-2];}
void Clamp(Zone& z,RECT const& a){int w=std::max(1L,a.right-a.left),h=std::max(1L,a.bottom-a.top); z.width=std::clamp(z.width,std::min(280,w),w); z.height=std::clamp(z.height,std::min(160,h),h); z.x=std::clamp(z.x,static_cast<int>(a.left),static_cast<int>(a.right)-z.width); int view=z.collapsed?std::min(88,h):ZoneExpandedHeight(z.height,z.maxHeight,h); z.y=std::clamp(z.y,static_cast<int>(a.top),static_cast<int>(a.bottom)-view);}
static std::wstring Lower(std::wstring v){CharLowerBuffW(v.data(),static_cast<DWORD>(v.size()));return v;}
static std::wstring ExtOf(std::wstring const& path){auto p=std::filesystem::path(path).extension().wstring();if(!p.empty()&&p.front()==L'.')p.erase(p.begin());return Lower(p);}
void DefaultRules(Layout& l){if(!l.rules.empty())return;auto push=[&](std::wstring name,std::vector<std::wstring> exts){l.rules.push_back({NewId(),std::move(name),std::move(exts),{},L""});};push(i18n::Tr(L"文档"),{L"doc",L"docx",L"pdf",L"txt",L"ppt",L"pptx",L"xls",L"xlsx",L"md",L"csv"});push(i18n::Tr(L"图片"),{L"png",L"jpg",L"jpeg",L"gif",L"bmp",L"webp"});push(i18n::Tr(L"安装包"),{L"exe",L"msi",L"zip",L"rar",L"7z"});}
long long ClampSizeKb(long long v){if(v<0)return 0;return v>100000000LL?100000000LL:v;}
int ClampAgeDays(int v){if(v<0)return 0;return v>36500?36500:v;}
long long ArchiveIntervalSeconds(int mode){return mode==1?3600LL:mode==2?86400LL:0LL;}
long long NowEpoch(){FILETIME f{};GetSystemTimeAsFileTime(&f);long long t=(static_cast<long long>(f.dwHighDateTime)<<32)|f.dwLowDateTime;return t>116444736000000000LL?(t-116444736000000000LL)/10000000LL:0LL;}
bool ArchiveDue(int mode,long long lastRun,long long now){auto const span=ArchiveIntervalSeconds(mode);if(span<=0||now<=0)return false;if(lastRun<=0||lastRun>now)return true;return now-lastRun>=span;}
int ClampArchiveMode(int v){return (v>=0&&v<=2)?v:0;}
int ClampStackGrid(int v){return (v>=0&&v<=3)?v:0;}
int StackCells(int mode){return mode==1?9:mode==2?16:mode==3?25:1;}
int StackSide(int availPx,int mode){
 auto const grid=ClampStackGrid(mode);
 int const want=grid==1?3:grid==2?4:grid==3?5:1;
 if(want<=1||availPx<=0)return 1;
 int const fit=(availPx+2)/12;
 if(fit<2)return 1;
 return fit<want?fit:want;
}
static int HexDigit(wchar_t c){if(c>=L'0'&&c<=L'9')return c-L'0';if(c>=L'a'&&c<=L'f')return c-L'a'+10;if(c>=L'A'&&c<=L'F')return c-L'A'+10;return -1;}
// 链接里的文件名要能直接当 Windows 文件名用：百分号编码还原、去禁用字符、去查询串、保留设备名加下划线
std::wstring DownloadName(std::wstring const& url,std::wstring const& contentType){
 auto const q=url.find_first_of(L"?#");std::wstring tail=q==std::wstring::npos?url:url.substr(0,q);
 size_t slash=tail.find_last_of(L"/\\");std::wstring name=slash==std::wstring::npos?std::wstring():tail.substr(slash+1);
 std::wstring out;for(size_t i=0;i<name.size();++i){wchar_t c=name[i];if(c==L'%'&&i+2<name.size()){int hi=HexDigit(name[i+1]),lo=HexDigit(name[i+2]);if(hi>=0&&lo>=0){out.push_back(static_cast<wchar_t>((hi<<4)|lo));i+=2;continue;}}out.push_back(c);}
 std::wstring clean;for(auto c:out)if(c>=32&&wcschr(L"\\/:*?\"<>|",c)==nullptr)clean.push_back(c);
 while(!clean.empty()&&(clean.front()==L' '||clean.front()==L'.'))clean.erase(clean.begin());
 while(!clean.empty()&&(clean.back()==L' '||clean.back()==L'.'||clean.back()==L'/'))clean.pop_back();
 if(clean.size()>80)clean.resize(80);
 while(!clean.empty()&&(clean.back()==L' '||clean.back()==L'.'))clean.pop_back();
 if(clean.empty())clean=L"download";
 auto const dot=clean.find_last_of(L'.');std::wstring stem=dot==std::wstring::npos||dot==0?clean:clean.substr(0,dot);
 auto const ext=dot==std::wstring::npos||dot==0?std::wstring():clean.substr(dot);
 if(stem.find_last_of(L".")!=std::wstring::npos||IsReservedDeviceName(stem))stem+=L"_";
 if(ext.empty()){auto const guess=ExtFromContentType(contentType);if(!guess.empty())return stem+L"."+guess;}
 return stem+ext;
}
std::wstring ExtFromContentType(std::wstring const& type){
 static const std::pair<wchar_t const*,wchar_t const*> map[]={
  {L"image/png",L"png"},{L"image/jpeg",L"jpg"},{L"image/gif",L"gif"},{L"image/webp",L"webp"},{L"image/svg+xml",L"svg"},
  {L"application/pdf",L"pdf"},{L"application/zip",L"zip"},{L"application/x-7z-compressed",L"7z"},{L"application/json",L"json"},
  {L"application/octet-stream",L"bin"},{L"text/plain",L"txt"},{L"text/markdown",L"md"},{L"text/csv",L"csv"},{L"text/html",L"html"},
  {L"video/mp4",L"mp4"},{L"audio/mpeg",L"mp3"}};
 auto head=type;auto semi=head.find(L';');if(semi!=std::wstring::npos)head=head.substr(0,semi);
 head=Lower(head);for(auto const& p:map)if(head==p.first)return p.second;
 return std::wstring();
}
bool IsHttpUrl(std::wstring const& url){return url.starts_with(L"http://")||url.starts_with(L"https://");}
// 规则命中判定：ext 与 lowerName 以及规则里的 exts/keywords 都必须已经小写归一；sizeKb/ageDays 为负=度量不到
bool RuleMatches(Rule const& rule,std::wstring const& ext,std::wstring const& lowerName,long long sizeKb,long long ageDays){
 if(rule.exts.empty()&&rule.keywords.empty())return false;
 bool m=false;
 for(auto const& e:rule.exts)if(!e.empty()&&ext==e){m=true;break;}
 if(!m)for(auto const& k:rule.keywords)if(!k.empty()&&lowerName.find(k)!=std::wstring::npos){m=true;break;}
 if(!m)return false;
 if(rule.minSizeKb>0&&(sizeKb<0||sizeKb<rule.minSizeKb))return false;
 if(rule.maxSizeKb>0&&(sizeKb<0||sizeKb>rule.maxSizeKb))return false;
 if(rule.olderThanDays>0&&(ageDays<0||ageDays<rule.olderThanDays))return false;
 return true;
}
static void FileDims(std::wstring const& path,long long& sizeKb,long long& ageDays){
 sizeKb=-1;ageDays=-1;
 WIN32_FILE_ATTRIBUTE_DATA d{};
 if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&d))return;
 ULARGE_INTEGER bytes{};bytes.LowPart=d.nFileSizeLow;bytes.HighPart=d.nFileSizeHigh;
 sizeKb=static_cast<long long>(bytes.QuadPart/1024ULL);
 ULARGE_INTEGER written{},now{};written.LowPart=d.ftLastWriteTime.dwLowDateTime;written.HighPart=d.ftLastWriteTime.dwHighDateTime;
 FILETIME current{};GetSystemTimeAsFileTime(&current);now.LowPart=current.dwLowDateTime;now.HighPart=current.dwHighDateTime;
 if(now.QuadPart<written.QuadPart)return;
 ageDays=static_cast<long long>((now.QuadPart-written.QuadPart)/10000000ULL/86400ULL);
}
// 规则名要当子文件夹名用：去掉 Windows 不允许的字符、收尾的点与空格，保留设备名加下划线，空名回退
std::wstring CategoryFolder(std::wstring const& name,std::wstring const& fallback){
 std::wstring out;
 for(auto ch:name)if(ch>=32&&wcschr(L"\\/:*?\"<>|",ch)==nullptr)out.push_back(ch);
 while(!out.empty()&&out.front()==L' ')out.erase(out.begin());
 if(out.size()>60){out.resize(60);if(!out.empty()&&out.back()>=0xD800&&out.back()<=0xDBFF)out.pop_back();}
 while(!out.empty()&&(out.back()==L'.'||out.back()==L' '))out.pop_back();
 if(IsReservedDeviceName(out))out+=L"_";
 if(out.empty())out=fallback;
 return out;
}
static bool LooseFile(std::filesystem::path const& p){DWORD attr=GetFileAttributesW(p.c_str());if(attr==INVALID_FILE_ATTRIBUTES)return false;if(attr&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM))return false;return _wcsicmp(p.filename().c_str(),L"desktop.ini")!=0;}
// 递归时不进隐藏/系统目录（挡住 $RECYCLE.BIN、.git 之类）也不进重解析点（链接与junction会绕圈）
static bool WanderableDir(std::filesystem::path const& p){DWORD attr=GetFileAttributesW(p.c_str());if(attr==INVALID_FILE_ATTRIBUTES)return false;if(attr&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_REPARSE_POINT))return false;return true;}
static void CollectLoose(std::filesystem::path const& dir,bool recursive,size_t limit,std::vector<std::wstring>& out,int depth){if(out.size()>=limit||depth>8)return;std::error_code ec;std::filesystem::directory_iterator it(dir,ec);if(ec)return;for(auto const& e:it){if(out.size()>=limit)return;std::error_code de;if(e.is_directory(de)){if(recursive&&!de&&WanderableDir(e.path()))CollectLoose(e.path(),true,limit,out,depth+1);continue;}auto path=e.path().wstring();if(!LooseFile(path))continue;out.push_back(std::move(path));}}
std::vector<std::wstring> ListLooseFiles(std::wstring const& folder,bool recursive,size_t limit){std::vector<std::wstring> out;CollectLoose(std::filesystem::path(folder),recursive,limit==0?1:limit,out,0);std::sort(out.begin(),out.end(),[](auto const& a,auto const& b){return StrCmpLogicalW(a.c_str(),b.c_str())<0;});return out;}
std::vector<std::wstring> DesktopFileList(){std::vector<std::wstring> out;PWSTR p{};if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop,0,nullptr,&p))){out=ListLooseFiles(p);CoTaskMemFree(p);}if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_PublicDesktop,0,nullptr,&p))){auto extra=ListLooseFiles(p);for(auto const& f:extra)if(std::find(out.begin(),out.end(),f)==out.end())out.push_back(f);CoTaskMemFree(p);}return out;}
std::vector<PlanItem> BuildPlan(std::vector<Rule> const& rules,std::vector<Zone> const& zones,std::vector<std::wstring> const& files,std::vector<std::wstring>* unmatched){std::vector<PlanItem> plan;if(unmatched)unmatched->clear();std::vector<Rule> norm;for(auto const& r:rules){if(r.targetZone.empty()||r.exts.empty()&&r.keywords.empty())continue;if(std::find_if(zones.begin(),zones.end(),[&](auto const& z){return z.id==r.targetZone;})==zones.end())continue;Rule n=r;for(auto& e:n.exts)e=Lower(e);for(auto& k:n.keywords)k=Lower(k);norm.push_back(std::move(n));}
for(auto const& f:files){auto ext=ExtOf(f);auto fname=Lower(std::filesystem::path(f).filename().wstring());long long sizeKb=-1,ageDays=-1;FileDims(f,sizeKb,ageDays);bool hit=false;for(auto const& r:norm)if(RuleMatches(r,ext,fname,sizeKb,ageDays)){plan.push_back({f,r.name,r.targetZone});hit=true;break;}if(!hit&&unmatched)unmatched->push_back(f);}return plan;}
// 归档计划与整理预览的区别：把命中的文件按规则名分组到同级分类子文件夹，由调用方真实移动；规则集合先过 RulesForZone 收窄到本分区
std::vector<ArchiveGroup> ArchivePlan(std::vector<Rule> const& rules,std::vector<std::wstring> const& files,std::vector<std::wstring>* unmatched,std::wstring const& root){
 std::vector<ArchiveGroup> groups;
 if(unmatched)unmatched->clear();
 std::vector<Rule> norm;
 for(auto const& r:rules){if(r.name.empty()||r.exts.empty()&&r.keywords.empty())continue;Rule n=r;for(auto& e:n.exts)e=Lower(e);for(auto& k:n.keywords)k=Lower(k);norm.push_back(std::move(n));}
 for(auto const& f:files){
  auto ext=ExtOf(f);auto fname=Lower(std::filesystem::path(f).filename().wstring());
  long long sizeKb=-1,ageDays=-1;FileDims(f,sizeKb,ageDays);
  Rule const* hit=nullptr;
  for(auto const& r:norm)if(RuleMatches(r,ext,fname,sizeKb,ageDays)){hit=&r;break;}
  if(!hit){if(unmatched)unmatched->push_back(f);continue;}
  auto cat=CategoryFolder(hit->name,i18n::Tr(L"未分类"));
  // 递归列举会把已经归好类的项目再捡回来：它已经在自己的分类文件夹里，就当作办完，别再搬运一次
  if(!root.empty()){auto parent=std::filesystem::path(f).parent_path().wstring();if(PathKey(parent)==PathKey((std::filesystem::path(root)/cat).wstring()))continue;}
  auto it=std::find_if(groups.begin(),groups.end(),[&](auto const& g){return g.category==cat;});
  if(it==groups.end())it=groups.insert(groups.end(),ArchiveGroup{cat,{}});
  it->paths.push_back(f);
 }
 return groups;
}
std::vector<Rule> RulesForZone(std::vector<Rule> const& rules,std::wstring const& zoneId){
 std::vector<Rule> kept;
 for(auto const& r:rules)if(r.targetZone.empty()||r.targetZone==zoneId)kept.push_back(r);
 return kept;
}
int UnbindRules(std::vector<Rule>& rules,std::wstring const& zoneId){
 if(zoneId.empty())return 0;
 int cleared=0;
 for(auto& r:rules)if(r.targetZone==zoneId){r.targetZone.clear();++cleared;}
 return cleared;
}
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
int ToggleTodo(Widgets& w,std::wstring const& id){for(auto& t:w.todos)if(t.id==id){if(!t.done&&t.repeat>0){t.due=NextDue(t.due,t.repeat,DueFromOffset(0));t.reminded=false;return 2;}t.done=!t.done;return 1;}return 0;}
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
std::wstring TopologySignature(std::vector<MonitorArea> const& areas){
 std::vector<std::wstring> parts;
 for(auto const& a:areas){wchar_t b[128]{};swprintf_s(b,128,L"%s:%dx%d%+d%+d",a.device.c_str(),static_cast<int>(a.work.right-a.work.left),static_cast<int>(a.work.bottom-a.work.top),static_cast<int>(a.work.left),static_cast<int>(a.work.top));parts.push_back(b);}
 std::sort(parts.begin(),parts.end());
 std::wstring out;for(auto const& p:parts){if(!out.empty())out+=L';';out+=p;}
 return out;
}
bool ArchiveTopology(std::map<std::wstring,std::string>& archives,std::wstring const& key,std::string const& body,size_t maxArchives,size_t maxBytes){
 if(key.empty()||key.size()>512||body.empty()||body.size()>maxBytes)return false;
 archives[key]=body;
 while(archives.size()>maxArchives)archives.erase(archives.begin());
 return true;
}
bool TakeTopology(std::map<std::wstring,std::string> const& archives,std::wstring const& key,std::string& out){auto it=archives.find(key);if(it==archives.end())return false;out=it->second;return true;}
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
static const unsigned kLunarInfo[150]={
0x04bd8,0x04ae0,0x0a570,0x054d5,0x0d260,0x0d950,0x16554,0x056a0,0x09ad0,0x055d2,0x04ae0,0x0a5b6,0x0a4d0,0x0d250,0x1d255,
0x0b540,0x0d6a0,0x18da3,0x095b0,0x14977,0x04970,0x0a4b0,0x1b0b6,0x06a50,0x06d40,0x1ab54,0x02b60,0x09570,0x052f2,0x04970,
0x06566,0x0d4a0,0x0ea50,0x16a95,0x05ad0,0x02b60,0x186e3,0x092e0,0x1c8d7,0x0c950,0x0d4a0,0x1d8a6,0x0b550,0x056a0,0x1a5b4,
0x025d0,0x092d0,0x0d2b2,0x0a950,0x0b557,0x06ca0,0x0b550,0x15355,0x04db0,0x025b0,0x18573,0x052b0,0x0a9a8,0x0e950,0x06aa0,
0x0aea6,0x0ab50,0x04b60,0x0aae4,0x0a570,0x05260,0x0f263,0x0d950,0x05b57,0x056a0,0x096d0,0x04dd5,0x04ad0,0x0a4d0,0x0d4d4,
0x0d250,0x0d558,0x0b540,0x0b6a0,0x195a6,0x095b0,0x049b0,0x0a974,0x0a4b0,0x0b27a,0x06a50,0x06d40,0x1ad47,0x0ab60,0x09570,
0x04af5,0x04970,0x064b0,0x074a3,0x0ea50,0x06b58,0x05ac0,0x0ab60,0x096e5,0x092e0,0x0c960,0x0d954,0x0d4a0,0x0da50,0x07552,
0x056a0,0x0abb7,0x025d0,0x092d0,0x0cab5,0x0a950,0x0b4a0,0x0bca4,0x0ad50,0x055d9,0x04ba0,0x0a5b0,0x15176,0x05270,0x0a930,
0x07954,0x06aa0,0x0ad50,0x05b52,0x04b60,0x0a6e6,0x0a4f0,0x05260,0x0ea65,0x0d520,0x0daa0,0x076a3,0x096d0,0x04afb,0x04ad0,
0x0a4d0,0x1d0b6,0x0d250,0x0d520,0x0dd45,0x0b5a0,0x056d0,0x055b2,0x049b0,0x0a577,0x0a4b0,0x0aa50,0x1b255,0x06d20,0x0ada0};
static int LunarLeapMonth(int y){return kLunarInfo[y-1900]&0xf;}
static int LunarMonthDays(int y,int m){return (kLunarInfo[y-1900]&(0x10000>>m))?30:29;}
static int LunarLeapDays(int y){int lp=LunarLeapMonth(y);return lp&&lp<=12?((kLunarInfo[y-1900]&0x10000)?30:29):0;}
static int LunarYearDays(int y){int sum=348;for(unsigned i=0x8000;i>0x8;i>>=1)sum+=(kLunarInfo[y-1900]&i)?1:0;return sum+LunarLeapDays(y);}
static long DaysFromCivil(int y,int m,int d){y-=m<=2;long era=(y>=0?y:y-399)/400;unsigned long yoe=(unsigned long)(y-era*400);unsigned long doy=(153u*(m+(m>2?-3:9))+2)/5+d-1;unsigned long doe=yoe*365+yoe/4-yoe/100+doy;return era*146097+(long)doe-719468;}
LunarDate LunarFromSolar(int year,int month,int day){
 LunarDate out;
 if(year<1900||year>2050||month<1||month>12||day<1||day>31)return out;
 long offset=DaysFromCivil(year,month,day)-DaysFromCivil(1900,1,31);
 if(offset<0)return out;
 int y=1900;
 for(;y<=2049;++y){int dy=LunarYearDays(y);if(offset<dy)break;offset-=dy;}
 if(y>2049)return out;
 int leap=LunarLeapMonth(y),m=1;bool isLeap=false;
 for(;m<=12;++m){
  int dm=LunarMonthDays(y,m);
  if(offset<dm)break;
  offset-=dm;
  if(m==leap){int ld=LunarLeapDays(y);if(offset<ld){isLeap=true;break;}offset-=ld;}
 }
 out.year=y;out.month=m;out.day=static_cast<int>(offset)+1;out.leap=isLeap;out.valid=true;
 return out;
}
std::wstring LunarText(LunarDate const& date){
 if(!date.valid||date.month<1||date.month>12||date.day<1||date.day>30)return {};
 static const wchar_t* mn[12]={L"正月",L"二月",L"三月",L"四月",L"五月",L"六月",L"七月",L"八月",L"九月",L"十月",L"冬月",L"腊月"};
 std::wstring dn;
 if(date.day==10)dn=L"初十";else if(date.day==20)dn=L"二十";else if(date.day==30)dn=L"三十";
 else{static const wchar_t* up[3]={L"初",L"十",L"廿"};static const wchar_t* low[10]={L"一",L"二",L"三",L"四",L"五",L"六",L"七",L"八",L"九",L"十"};dn=std::wstring(up[date.day/10])+low[date.day%10-1];}
 return (date.leap?L"闰":L"")+std::wstring(mn[date.month-1])+dn;
}
static int QingmingDay(int y){int yy=y%100;double c=y<2000?4.81:5.59;return static_cast<int>(yy*0.2422+c)-yy/4;}
std::wstring HolidayText(int year,int month,int day,LunarDate const& lunar){
 if(lunar.valid&&!lunar.leap){
  if(lunar.month==1&&lunar.day==1)return L"春节";
  if(lunar.month==1&&lunar.day==15)return L"元宵节";
  if(lunar.month==2&&lunar.day==2)return L"龙抬头";
  if(lunar.month==5&&lunar.day==5)return L"端午节";
  if(lunar.month==7&&lunar.day==7)return L"七夕";
  if(lunar.month==8&&lunar.day==15)return L"中秋节";
  if(lunar.month==9&&lunar.day==9)return L"重阳节";
  if(lunar.month==12&&lunar.day==8)return L"腊八";
  if(lunar.month==12&&lunar.day==LunarMonthDays(lunar.year,12))return L"除夕";
 }
 if(month==1&&day==1)return L"元旦";
 if(month==3&&day==8)return L"妇女节";
 if(month==4&&day==QingmingDay(year))return L"清明节";
 if(month==5&&day==1)return L"劳动节";
 if(month==5&&day==4)return L"青年节";
 if(month==6&&day==1)return L"儿童节";
 if(month==8&&day==1)return L"建军节";
 if(month==9&&day==10)return L"教师节";
 if(month==10&&day==1)return L"国庆节";
 return {};
}
int ZoneColorCount(){return 8;}
unsigned ZoneColorRGB(int color){static const unsigned pal[8]={0xE81123,0xF7630C,0xFFB900,0x13A10E,0x00B7C3,0x0078D4,0x8764B8,0xE3008C};return color>=1&&color<=8?pal[color-1]:0u;}
int ClampOpacityPercent(int v){return v<20?20:(v>100?100:v);}
int NextOpacityStep(int v){if(v>=90)return 80;if(v>=70)return 60;if(v>=50)return 40;return 100;}
int ClampBackdropKind(int v){return v==1?1:0;}
int ClampExpandDir(int v){return v<0||v>3?0:v;}
int ClampMaxHeight(int v){return v<25?0:(v>100?100:v);}
int ZoneCapHeight(int maxHeight,int workHeight){if(maxHeight<=0||workHeight<=0)return 1<<20;int cap=static_cast<int>(static_cast<long long>(workHeight)*maxHeight/100);return cap<88?88:cap;}
int ZoneExpandedHeight(int height,int maxHeight,int workHeight){int cap=ZoneCapHeight(maxHeight,workHeight);return height>cap?cap:height;}
int ZoneAnchorTop(int top,int height,int targetHeight,int dir,int workTop,int workBottom){
 int bottom=top+height;int t=ClampExpandDir(dir);int next;
 if(t==1)next=top;
 else if(t==2)next=bottom-targetHeight;
 else if(t==3)next=top+(height-targetHeight)/2;
 else if(top+targetHeight<=workBottom)next=top;
 else if(bottom-targetHeight>=workTop)next=bottom-targetHeight;
 else next=top+(height-targetHeight)/2;
 int work=workBottom-workTop;
 if(targetHeight>0&&work>0&&targetHeight>=work)return workTop;
 if(next+targetHeight>workBottom)next=workBottom-targetHeight;
 if(next<workTop)next=workTop;
 return next;}
wchar_t const* ExpandDirName(int dir){switch(ClampExpandDir(dir)){case 1:return L"向下展开";case 2:return L"向上展开";case 3:return L"居中展开";default:return L"自动展开";}}
std::wstring MediaTrackLine(std::wstring const& title,std::wstring const& artist){
 auto trim=[](std::wstring s){size_t b=s.find_first_not_of(L" \t\r\n");if(b==std::wstring::npos)return std::wstring{};size_t e=s.find_last_not_of(L" \t\r\n");return s.substr(b,e-b+1);};
 auto t=trim(title),a=trim(artist);
 if(t.empty())return a;
 if(a.empty())return t;
 auto line=t+L" · "+a;
 if(line.size()>160)line=line.substr(0,159)+L"…";
 return line;
}
std::wstring MediaTimeText(long long posSec,long long endSec){
 if(posSec<0)posSec=0;if(endSec<0)endSec=0;
 wchar_t buf[48]{};
 if(endSec==0)swprintf_s(buf,48,L"%lld:%02lld",posSec/60,posSec%60);
 else swprintf_s(buf,48,L"%lld:%02lld / %lld:%02lld",posSec/60,posSec%60,endSec/60,endSec%60);
 return buf;
}
int MediaStatusKind(int raw){switch(raw){case 0:return 0;case 4:return 3;case 5:return 2;default:return 1;}}
bool IsImagePath(std::wstring const& path){auto f=path.find_last_of(L'.');if(f==std::wstring::npos||f+1>=path.size())return false;auto ext=path.substr(f+1);if(ext.size()>5||ext.find_first_of(L"\\/")!=std::wstring::npos)return false;for(auto& c:ext)c=towlower(c);return ext==L"png"||ext==L"jpg"||ext==L"jpeg"||ext==L"bmp"||ext==L"gif"||ext==L"webp"||ext==L"tif"||ext==L"tiff";}
std::vector<std::wstring> FilterImagePaths(std::vector<std::wstring> const& paths){std::vector<std::wstring> out;for(auto const& p:paths)if(IsImagePath(p))out.push_back(p);return out;}
bool IsTextPath(std::wstring const& path){static const wchar_t* table[]{L"txt",L"md",L"markdown",L"log",L"csv",L"tsv",L"json",L"jsonc",L"xml",L"xaml",L"ini",L"cfg",L"conf",L"reg",L"properties",L"yaml",L"yml",L"toml",L"plist",L"html",L"htm",L"css",L"js",L"jsx",L"mjs",L"cjs",L"ts",L"tsx",L"c",L"cpp",L"cc",L"cxx",L"h",L"hpp",L"hxx",L"cs",L"vb",L"java",L"kt",L"scala",L"go",L"rs",L"rb",L"py",L"php",L"pl",L"lua",L"sh",L"bash",L"zsh",L"ps1",L"psm1",L"bat",L"cmd",L"sql",L"srt",L"vtt",L"ass",L"diff",L"patch",L"mk",L"cmake",L"gradle",L"svg",L"tex",L"ics",L"env"};auto e=ExtOf(path);if(e.empty())return false;for(auto const* x:table)if(e==x)return true;return false;}
int PreviewKind(std::wstring const& path){if(IsImagePath(path))return 1;if(IsTextPath(path))return 2;return 0;}
static std::wstring NormalizeNewlines(std::wstring const& s){std::wstring out;out.reserve(s.size());for(size_t i=0;i<s.size();++i){if(s[i]!=L'\r'){out.push_back(s[i]);continue;}if(i+1<s.size()&&s[i+1]==L'\n')++i;out.push_back(L'\n');}return out;}
std::wstring DecodeNeutralText(std::vector<char> const& raw){
 size_t off=0;bool le=false,be=false;
 if(raw.size()>=3&&(unsigned char)raw[0]==0xEF&&(unsigned char)raw[1]==0xBB&&(unsigned char)raw[2]==0xBF)off=3;
 else if(raw.size()>=2&&(unsigned char)raw[0]==0xFF&&(unsigned char)raw[1]==0xFE){le=true;off=2;}
 else if(raw.size()>=2&&(unsigned char)raw[0]==0xFE&&(unsigned char)raw[1]==0xFF){be=true;off=2;}
 if(!le&&!be){
  auto const* p=raw.data()+off;
  int const len=static_cast<int>(raw.size()-off);
  // 预览只读取文件头部，截断处可能劈开一个多字节字符：先回退尾部 1..3 字节再严格解码，避免整篇退成乱码
  int n=0,used=len;
  for(int trim=0;trim<=3&&n<=0;++trim){
   used=len-trim;
   if(used<=0)break;
   n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,p,used,nullptr,0);
  }
  UINT cp=CP_UTF8,flags=MB_ERR_INVALID_CHARS;
  if(n<=0){cp=CP_ACP;flags=0;used=len;n=MultiByteToWideChar(cp,flags,p,used,nullptr,0);}
  if(n<=0)return L"";
  std::wstring out(n,L'\0');
  auto wrote=MultiByteToWideChar(cp,flags,p,used,out.data(),n);
  if(wrote<=0)return L"";
  out.resize(wrote);
  return NormalizeNewlines(out);
 }
 size_t units=(raw.size()-off)/2;if(units==0)return L"";
 std::wstring out(units,L'\0');memcpy(out.data(),raw.data()+off,units*sizeof(wchar_t));
 if(be)for(auto& c:out)c=static_cast<wchar_t>((static_cast<unsigned>(c)>>8)|(static_cast<unsigned>(c&0xFF)<<8));
 return NormalizeNewlines(out);
}
std::wstring ClampPreviewText(std::wstring const& text,size_t maxChars){if(maxChars==0||text.size()<=maxChars)return text;auto cut=text.substr(0,maxChars);auto nl=cut.rfind(L'\n');if(nl!=std::wstring::npos&&nl>maxChars/2)cut=cut.substr(0,nl);return cut;}
std::wstring PreviewSizeText(long long bytes){if(bytes<0)bytes=0;wchar_t buf[64]{};if(bytes<1024){swprintf_s(buf,64,L"%lld B",bytes);return buf;}if(bytes<1024LL*1024){swprintf_s(buf,64,L"%.1f KB",bytes/1024.0);return buf;}if(bytes<1024LL*1024*1024){swprintf_s(buf,64,L"%.1f MB",bytes/(1024.0*1024.0));return buf;}swprintf_s(buf,64,L"%.2f GB",bytes/(1024.0*1024.0*1024.0));return buf;}
int ProgressPercent(long long done,long long total){
 if(total<=0||done<=0)return 0;
 if(done>=total)return 100;
 return static_cast<int>((done*100+total/2)/total);
}
int DropOperation(bool mappedZone,bool shiftPressed){return mappedZone?(shiftPressed?2:1):0;}
int OpOutcome(long long ok,long long failed,bool cancelled){
 if(cancelled&&ok==0&&failed==0)return 4;
 if(ok>0&&failed>0)return 2;
 if(failed>0)return 3;
 if(ok>0)return 1;
 return 0;
}
bool IsReservedDeviceName(std::wstring const& name){
 auto dot=name.find_first_of(L".");
 auto stem=Lower(name.substr(0,dot==std::wstring::npos?name.size():dot));
 if(stem.empty())return false;
 static wchar_t const* plain[]{L"con",L"prn",L"aux",L"nul",L"clock$"};
 for(auto const* p:plain)if(stem==p)return true;
 if(stem.size()==4&&(stem==L"com1"||stem==L"com2"||stem==L"com3"||stem==L"com4"||stem==L"com5"||stem==L"com6"||stem==L"com7"||stem==L"com8"||stem==L"com9"||stem==L"lpt1"||stem==L"lpt2"||stem==L"lpt3"||stem==L"lpt4"||stem==L"lpt5"||stem==L"lpt6"||stem==L"lpt7"||stem==L"lpt8"||stem==L"lpt9"))return true;
 return false;
}
std::wstring BuildDiagnostics(Layout const& l,std::wstring const& version,std::wstring const& machine,std::wstring const& osBuild,long long today){
 auto b=[](bool v){return v?std::wstring(L"1"):std::wstring(L"0");};
 auto n=[](size_t v){return std::to_wstring(v);};
 auto orNone=[](std::wstring const& v,wchar_t const* def){return v.empty()?std::wstring(def):v;};
 size_t entries=0,stacks=0,pins=0,mapped=0,capsules=0,collapsed=0,grouped=0,colored=0,bg=0;
 for(auto const& z:l.zones){entries+=z.entries.size();stacks+=z.stacks.size();pins+=z.pins.size();if(!z.mappedFolder.empty())++mapped;if(z.capsule)++capsules;if(z.collapsed)++collapsed;if(!z.group.empty())++grouped;if(z.color)++colored;if(!z.background.empty())++bg;}
 size_t done=0,overdue=0,repeating=0,flags=0;
 for(auto const& t:l.widgets.todos){if(t.done)++done;if(!t.done&&t.due>0&&t.due<today)++overdue;if(t.repeat)++repeating;if(t.flag)++flags;}
 std::wstring s=L"GuoDesk Diagnostics\n";
 s+=L"version="+version+L"\nmachine="+machine+L"\nos="+orNone(osBuild,L"unknown")+L"\n\n";
 s+=L"theme="+orNone(l.settings.theme,L"System")+L" language="+orNone(l.settings.language,L"zh")+L" textSize="+n(l.settings.textSize)+L" clockStyle="+(l.settings.clockStyle.empty()?L"digital":l.settings.clockStyle)+L" backdrop="+n(l.settings.backdrop)+L"\n";
 s+=L"performance="+b(l.settings.performance)+L" memTrim="+b(l.settings.memTrim)+L" snapshots="+b(l.settings.snapshots)+L" tabHover="+b(l.settings.tabHover)+L" guideDone="+b(l.settings.guideDone)+L" everything="+b(l.settings.everything)+L"\n";
 s+=L"hotkey="+orNone(l.settings.hotkey,L"none")+L" search="+orNone(l.settings.hotkeySearch,L"none")+L" capture="+orNone(l.settings.hotkeyCapture,L"none")+L" undo="+orNone(l.settings.hotkeyUndo,L"none")+L" reveal="+orNone(l.settings.revealHotkey,L"none")+L" raise="+orNone(l.settings.hotkeyRaise,L"none")+L"\n\n";
 s+=L"zones="+n(l.zones.size())+L" entries="+n(entries)+L" stacks="+n(stacks)+L" pins="+n(pins)+L" mapped="+n(mapped)+L" capsules="+n(capsules)+L" collapsed="+n(collapsed)+L" grouped="+n(grouped)+L" colored="+n(colored)+L" backgrounds="+n(bg)+L" rules="+n(l.rules.size())+L"\n";
 s+=L"todos="+n(l.widgets.todos.size())+L" done="+n(done)+L" overdue="+n(overdue)+L" repeating="+n(repeating)+L" flags="+n(flags)+L" notePages="+n(l.widgets.pages.size())+L" searchHistory="+n(l.settings.searchHistory.size())+L" searchFavorites="+n(l.settings.searchFavorites.size())+L"\n";
 s+=L"widgets note="+b(l.widgets.noteVisible)+L" todo="+b(l.widgets.todoVisible)+L" clock="+b(l.widgets.clockVisible)+L" music="+b(l.widgets.musicVisible)+L" weather="+b(l.widgets.weatherVisible)+L" appGrid="+b(l.widgets.appGridVisible)+L" clockBg="+(l.widgets.clockBg.empty()?L"off":L"on")+L"\n";
 s+=std::wstring(L"webdav=")+(l.settings.syncUrl.empty()?L"off":L"configured")+L" syncAuto="+b(l.settings.syncAuto)+L" topologyArchives="+n(l.topologyArchives.size())+L" topologyLast="+(l.topologyLast.empty()?L"none":L"known")+L"\n";
 return s;
}
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
long long NextDue(long long due,int repeat,long long today){
 if(repeat<1||repeat>4)return due;
 long long start=due>0?due:today;
 static const int dm[]{31,28,31,30,31,30,31,31,30,31,30,31};
 auto add=[&](long long s){
  if(repeat==4){int y=static_cast<int>(s/10000),m=static_cast<int>(s/100%100),d=static_cast<int>(s%100);++m;if(m>12){m=1;++y;}int max=dm[m-1];if(m==2&&((y%4==0&&y%100!=0)||y%400==0))max=29;if(d>max)d=max;return static_cast<long long>(y)*10000+m*100+d;}
  int days=repeat==1?1:repeat==2?7:14;
  long long t=StampToFT(s)+static_cast<long long>(days)*864000000000LL;
  FILETIME f{static_cast<DWORD>(t&0xFFFFFFFF),static_cast<DWORD>(t>>32)};
  SYSTEMTIME out{};FileTimeToSystemTime(&f,&out);
  return StampFromST(out);
 };
 long long next=add(start);
 for(int guard=0;next<today&&guard<5000;++guard)next=add(next);
 return next;
}
int TodoFlagCount(){return 4;}
unsigned TodoFlagRGB(int flag){switch(flag){case 1:return 0xE81123;case 2:return 0xFFB900;case 3:return 0x13A10E;default:return 0;}}
bool TodoMatchesFilter(TodoItem const& t,int filter){
 if(filter==1)return !t.done;
 if(filter==2)return t.done;
 if(filter==3)return !t.done&&t.due>0&&t.due<DueFromOffset(0);
 if(filter==4)return t.repeat>0;
 return true;
}
std::vector<MdSeg> ParseInlineMarkdown(std::wstring const& s){
 std::vector<MdSeg> out;std::wstring plain;size_t i=0;
 auto em=[&](std::wstring const& t,int st){if(!t.empty())out.push_back({t,st});};
 static const std::pair<wchar_t const*,int> marks[]={ {L"**",1},{L"~~",2},{L"`",3} };
 while(i<s.size()){
  bool hit=false;
  for(auto const& mk:marks){
   size_t n=wcslen(mk.first);
   if(s.compare(i,n,mk.first)==0){
    auto e=s.find(mk.first,i+n);
    if(e!=std::wstring::npos&&e>i+n){em(plain,0);plain.clear();em(s.substr(i+n,e-i-n),mk.second);i=e+n;hit=true;break;}
   }
  }
  if(hit)continue;
  if(s[i]==L'#'&&(i==0||s[i-1]==L' ')){
   size_t j=i+1;while(j<s.size()&&s[j]!=L' '&&s[j]!=L'\n')++j;
   if(j>i+1){em(plain,0);plain.clear();em(s.substr(i,j-i),4);i=j;continue;}
  }
  plain+=s[i++];
 }
 em(plain,0);
 if(out.empty())out.push_back({L"",0});
 return out;
}
bool TodoMatchesQuery(TodoItem const& t,std::wstring const& q){
 if(q.empty())return true;
 auto a=t.text,b=q;
 for(auto& ch:a)ch=static_cast<wchar_t>(towlower(ch));
 for(auto& ch:b)ch=static_cast<wchar_t>(towlower(ch));
 return a.find(b)!=std::wstring::npos;
}
int RemoveTodos(Widgets& w,std::vector<std::wstring> const& ids){
 size_t before=w.todos.size();
 std::erase_if(w.todos,[&](auto const& t){return std::find(ids.begin(),ids.end(),t.id)!=ids.end();});
 return static_cast<int>(before-w.todos.size());
}
int ClearDoneTodos(Widgets& w){size_t before=w.todos.size();std::erase_if(w.todos,[](auto const& t){return t.done;});return static_cast<int>(before-w.todos.size());}
bool ShouldListApp(std::wstring const& name,std::wstring const& target){if(name.empty()||target.empty())return false;auto n=Lower(name);if(n.find(L"unins")!=std::wstring::npos)return false;if(n.starts_with(L"卸载")||n.starts_with(L"卸 载"))return false;return true;}
bool AppMatches(std::wstring const& name,std::wstring const& query){if(query.empty())return true;return Lower(name).find(Lower(query))!=std::wstring::npos;}
void SortAppsByName(std::vector<AppShortcut>& apps){std::stable_sort(apps.begin(),apps.end(),[](auto const& a,auto const& b){return Lower(a.name)<Lower(b.name);});}
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
static std::wstring TrimQuery(std::wstring const& q){size_t b=q.find_first_not_of(L" \t\r\n");if(b==std::wstring::npos)return L"";size_t e=q.find_last_not_of(L" \t\r\n");return q.substr(b,e-b+1);}
void PushSearchHistory(std::vector<std::wstring>& history,std::wstring const& query,size_t limit){
 auto q=TrimQuery(query);
 if(q.empty())return;
 history.erase(std::remove_if(history.begin(),history.end(),[&](std::wstring const& s){return s==q;}),history.end());
 history.insert(history.begin(),q);
 if(history.size()>limit)history.resize(limit);
}
bool ToggleSearchFavorite(std::vector<std::wstring>& favorites,std::wstring const& query,size_t limit){
 auto q=TrimQuery(query);
 if(q.empty())return false;
 auto it=std::find(favorites.begin(),favorites.end(),q);
 if(it!=favorites.end()){favorites.erase(it);return false;}
 favorites.insert(favorites.begin(),q);
 if(favorites.size()>limit)favorites.resize(limit);
 return true;
}
bool ToggleSelect(std::vector<std::wstring>& keys,std::wstring const& key,size_t limit){
 if(key.empty())return false;
 auto it=std::find(keys.begin(),keys.end(),key);
 if(it!=keys.end()){keys.erase(it);return false;}
 keys.push_back(key);
 if(keys.size()>limit)keys.erase(keys.begin());
 return true;
}
int SelectRange(std::vector<std::wstring>& keys,std::vector<std::wstring> const& paths,int anchor,int target,size_t limit){
 if(paths.empty())return 0;
 auto inside=[](int i,int n){return i<0?0:(i>=n?n-1:i);};
 int a=inside(anchor,(int)paths.size()),b=inside(target,(int)paths.size());
 if(a>b)std::swap(a,b);
 int added=0;
 for(int i=a;i<=b;++i){auto key=PathKey(paths[i]);if(std::find(keys.begin(),keys.end(),key)!=keys.end())continue;added+=ToggleSelect(keys,key,limit)?1:0;}
 return added;
}
std::wstring UniqueName(std::vector<std::wstring> const& taken,std::wstring const& base,std::wstring const& ext){
 auto free=[&](std::wstring const& name){auto want=Lower(name);return !std::any_of(taken.begin(),taken.end(),[&](auto const& t){return Lower(t)==want;});};
 if(free(base+ext))return base+ext;
 for(int n=2;n<=999;++n){auto name=base+L" ("+std::to_wstring(n)+L")"+ext;if(free(name))return name;}
 return base+L" (1000)"+ext;
}
int NavStep(int cur,int count,int delta){
 if(count<=0)return -1;
 if(cur<0)return delta<0?count-1:0;
 int next=cur+delta;
 if(next<0)next=0;
 if(next>=count)next=count-1;
 return next;
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
int WeatherSkinCount(){return 7;}
unsigned WeatherSkinTop(int skin){static const unsigned t[]={0x3A7BD3,0x0F203E,0x6B798A,0x3A609F,0x83A5BE,0x2B2D4F,0x7F8D99};return skin>=0&&skin<WeatherSkinCount()?t[skin]:t[2];}
unsigned WeatherSkinBottom(int skin){static const unsigned b[]={0x9BE1FF,0x3E5C8A,0xB8C4CE,0x8EA8C0,0xE3EEF7,0x63668F,0xC5CED5};return skin>=0&&skin<WeatherSkinCount()?b[skin]:b[2];}
int WeatherSkinForCode(int code,bool night){
 if(code<=1)return night?1:0;
 if(code<=3)return 2;
 if(code==45||code==48)return 6;
 if(code>=95)return 5;
 if((code>=71&&code<=77)||code==85||code==86)return 4;
 return 3;
}
long long NowHourKey(){SYSTEMTIME st{};GetLocalTime(&st);return (static_cast<long long>(st.wYear)*10000+st.wMonth*100+st.wDay)*100+st.wHour;}
WeatherNow ParseWeatherJson(std::wstring const& json,long long nowKey){
 WeatherNow w;
 try{
  auto root=JsonObject::Parse(json);
  auto cur=root.GetNamedObject(L"current");
  w.temp=cur.GetNamedNumber(L"temperature_2m");
  w.code=static_cast<int>(cur.GetNamedNumber(L"weather_code"));
  if(cur.HasKey(L"relative_humidity_2m"))w.humidity=static_cast<int>(cur.GetNamedNumber(L"relative_humidity_2m"));
  if(cur.HasKey(L"is_day"))w.isDay=cur.GetNamedNumber(L"is_day")!=0;
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
  for(uint32_t i=1;i<times.Size()&&w.days.size()<6;++i)if(i<codes.Size()&&i<his.Size()&&i<los.Size())w.days.push_back({static_cast<int>(codes.GetAt(i).GetNumber()),los.GetAt(i).GetNumber(),his.GetAt(i).GetNumber(),isoDate(i)});
  if(root.HasKey(L"hourly")){
   try{
    auto hr=root.GetNamedObject(L"hourly");
    auto ht=hr.GetNamedArray(L"time");auto hc=hr.GetNamedArray(L"weather_code");auto htmp=hr.GetNamedArray(L"temperature_2m");
    auto nk=nowKey?nowKey:NowHourKey();
    for(uint32_t i=0;i<ht.Size()&&w.hours.size()<12;++i){
     try{
      auto s=ht.GetAt(i).GetString();
      if(s.size()<13)continue;
      auto num=[&](uint32_t a,uint32_t b)->int{int v=0;for(uint32_t k=a;k<b;++k){auto c=s[k];if(c<L'0'||c>L'9')return -1;v=v*10+(c-L'0');}return v;};
      long long key=(static_cast<long long>(num(0,4))*10000+num(5,7)*100+num(8,10))*100+num(11,13);
      if(key<nk)continue;
      WeatherHour h;h.key=key;h.code=static_cast<int>(hc.GetAt(i).GetNumber());h.temp=htmp.GetAt(i).GetNumber();
      w.hours.push_back(h);
     }catch(...){}
    }
   }catch(...){}
  }
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
std::string Serialize(Layout const& l){JsonObject root; root.SetNamedValue(L"version",JsonValue::CreateNumberValue(1)); JsonArray zones; for(auto const& z:l.zones){JsonObject j; j.SetNamedValue(L"id",JsonValue::CreateStringValue(z.id)); j.SetNamedValue(L"name",JsonValue::CreateStringValue(z.name)); j.SetNamedValue(L"x",JsonValue::CreateNumberValue(z.x));j.SetNamedValue(L"y",JsonValue::CreateNumberValue(z.y));j.SetNamedValue(L"width",JsonValue::CreateNumberValue(z.width));j.SetNamedValue(L"height",JsonValue::CreateNumberValue(z.height));j.SetNamedValue(L"collapsed",JsonValue::CreateBooleanValue(z.collapsed)); j.SetNamedValue(L"mappedFolder",JsonValue::CreateStringValue(z.mappedFolder)); j.SetNamedValue(L"viewMode",JsonValue::CreateStringValue(z.viewMode)); j.SetNamedValue(L"nameLines",JsonValue::CreateNumberValue(z.nameLines)); j.SetNamedValue(L"sortKey",JsonValue::CreateStringValue(z.sortKey)); j.SetNamedValue(L"sortDescending",JsonValue::CreateBooleanValue(z.sortDescending)); j.SetNamedValue(L"tileSize",JsonValue::CreateNumberValue(z.tileSize)); j.SetNamedValue(L"mon",JsonValue::CreateStringValue(z.mon)); j.SetNamedValue(L"mx",JsonValue::CreateNumberValue(z.mx)); j.SetNamedValue(L"my",JsonValue::CreateNumberValue(z.my)); j.SetNamedValue(L"group",JsonValue::CreateStringValue(z.group)); j.SetNamedValue(L"groupTab",JsonValue::CreateNumberValue(z.groupTab)); j.SetNamedValue(L"capsule",JsonValue::CreateBooleanValue(z.capsule)); j.SetNamedValue(L"browseInPlace",JsonValue::CreateBooleanValue(z.browseInPlace)); j.SetNamedValue(L"browseFolder",JsonValue::CreateStringValue(z.browseFolder)); j.SetNamedValue(L"color",JsonValue::CreateNumberValue(z.color)); j.SetNamedValue(L"background",JsonValue::CreateStringValue(z.background)); j.SetNamedValue(L"dim",JsonValue::CreateNumberValue(z.dim)); JsonArray pinsArr;for(auto const& p:z.pins)pinsArr.Append(JsonValue::CreateStringValue(p));j.SetNamedValue(L"pins",pinsArr); j.SetNamedValue(L"opacity",JsonValue::CreateNumberValue(z.opacity)); j.SetNamedValue(L"expandDir",JsonValue::CreateNumberValue(z.expandDir)); j.SetNamedValue(L"maxHeight",JsonValue::CreateNumberValue(z.maxHeight)); j.SetNamedValue(L"locked",JsonValue::CreateBooleanValue(z.locked)); j.SetNamedValue(L"autoArchive",JsonValue::CreateNumberValue(z.autoArchive)); j.SetNamedValue(L"archiveAt",JsonValue::CreateNumberValue(static_cast<double>(z.archiveAt))); j.SetNamedValue(L"stackGrid",JsonValue::CreateNumberValue(z.stackGrid)); JsonArray stacksArr;for(auto const& st:z.stacks){JsonObject sj;sj.SetNamedValue(L"id",JsonValue::CreateStringValue(st.id));sj.SetNamedValue(L"name",JsonValue::CreateStringValue(st.name));stacksArr.Append(sj);}j.SetNamedValue(L"stacks",stacksArr); JsonArray entries;for(auto const& e:z.entries){JsonObject item;item.SetNamedValue(L"id",JsonValue::CreateStringValue(e.id));item.SetNamedValue(L"path",JsonValue::CreateStringValue(e.path));item.SetNamedValue(L"stack",JsonValue::CreateStringValue(e.stack));entries.Append(item);}j.SetNamedValue(L"entries",entries);zones.Append(j);}root.SetNamedValue(L"zones",zones);JsonObject s;s.SetNamedValue(L"theme",JsonValue::CreateStringValue(l.settings.theme));s.SetNamedValue(L"compact",JsonValue::CreateBooleanValue(l.settings.compact));s.SetNamedValue(L"language",JsonValue::CreateStringValue(l.settings.language)); s.SetNamedValue(L"hotkey",JsonValue::CreateStringValue(l.settings.hotkey)); s.SetNamedValue(L"snapshots",JsonValue::CreateBooleanValue(l.settings.snapshots));s.SetNamedValue(L"guideDone",JsonValue::CreateBooleanValue(l.settings.guideDone));s.SetNamedValue(L"hotkeySearch",JsonValue::CreateStringValue(l.settings.hotkeySearch));s.SetNamedValue(L"hotkeyCapture",JsonValue::CreateStringValue(l.settings.hotkeyCapture));s.SetNamedValue(L"performance",JsonValue::CreateBooleanValue(l.settings.performance));s.SetNamedValue(L"syncUrl",JsonValue::CreateStringValue(l.settings.syncUrl));s.SetNamedValue(L"syncUser",JsonValue::CreateStringValue(l.settings.syncUser));s.SetNamedValue(L"syncPass",JsonValue::CreateStringValue(l.settings.syncPass));s.SetNamedValue(L"syncAuto",JsonValue::CreateBooleanValue(l.settings.syncAuto));s.SetNamedValue(L"syncInsecure",JsonValue::CreateBooleanValue(l.settings.syncInsecure));s.SetNamedValue(L"textSize",JsonValue::CreateNumberValue(l.settings.textSize));s.SetNamedValue(L"clockStyle",JsonValue::CreateStringValue(l.settings.clockStyle));s.SetNamedValue(L"everything",JsonValue::CreateBooleanValue(l.settings.everything));s.SetNamedValue(L"hotkeyUndo",JsonValue::CreateStringValue(l.settings.hotkeyUndo));s.SetNamedValue(L"tabHover",JsonValue::CreateBooleanValue(l.settings.tabHover));s.SetNamedValue(L"memTrim",JsonValue::CreateBooleanValue(l.settings.memTrim));s.SetNamedValue(L"revealHotkey",JsonValue::CreateStringValue(l.settings.revealHotkey));s.SetNamedValue(L"hotkeyRaise",JsonValue::CreateStringValue(l.settings.hotkeyRaise));s.SetNamedValue(L"backdrop",JsonValue::CreateNumberValue(l.settings.backdrop));{JsonArray sh;for(auto const& q:l.settings.searchHistory)sh.Append(JsonValue::CreateStringValue(q));s.SetNamedValue(L"searchHistory",sh);JsonArray sf;for(auto const& q:l.settings.searchFavorites)sf.Append(JsonValue::CreateStringValue(q));s.SetNamedValue(L"searchFavorites",sf);}root.SetNamedValue(L"settings",s);JsonArray rs;for(auto const& r:l.rules){JsonObject rj;rj.SetNamedValue(L"id",JsonValue::CreateStringValue(r.id));rj.SetNamedValue(L"name",JsonValue::CreateStringValue(r.name));JsonArray es;for(auto const& e:r.exts)es.Append(JsonValue::CreateStringValue(e));rj.SetNamedValue(L"exts",es);JsonArray ks;for(auto const& k:r.keywords)ks.Append(JsonValue::CreateStringValue(k));rj.SetNamedValue(L"keywords",ks);rj.SetNamedValue(L"zone",JsonValue::CreateStringValue(r.targetZone));rj.SetNamedValue(L"minSize",JsonValue::CreateNumberValue(static_cast<double>(ClampSizeKb(r.minSizeKb))));rj.SetNamedValue(L"maxSize",JsonValue::CreateNumberValue(static_cast<double>(ClampSizeKb(r.maxSizeKb))));rj.SetNamedValue(L"olderThan",JsonValue::CreateNumberValue(static_cast<double>(ClampAgeDays(r.olderThanDays))));rs.Append(rj);}root.SetNamedValue(L"rules",rs);JsonObject wj;wj.SetNamedValue(L"noteVisible",JsonValue::CreateBooleanValue(l.widgets.noteVisible));wj.SetNamedValue(L"todoVisible",JsonValue::CreateBooleanValue(l.widgets.todoVisible));wj.SetNamedValue(L"noteText",JsonValue::CreateStringValue(l.widgets.noteText));wj.SetNamedValue(L"noteX",JsonValue::CreateNumberValue(l.widgets.noteX));wj.SetNamedValue(L"noteY",JsonValue::CreateNumberValue(l.widgets.noteY));wj.SetNamedValue(L"noteW",JsonValue::CreateNumberValue(l.widgets.noteW));wj.SetNamedValue(L"noteH",JsonValue::CreateNumberValue(l.widgets.noteH));wj.SetNamedValue(L"todoX",JsonValue::CreateNumberValue(l.widgets.todoX));wj.SetNamedValue(L"todoY",JsonValue::CreateNumberValue(l.widgets.todoY));wj.SetNamedValue(L"todoW",JsonValue::CreateNumberValue(l.widgets.todoW));wj.SetNamedValue(L"todoH",JsonValue::CreateNumberValue(l.widgets.todoH));wj.SetNamedValue(L"noteTop",JsonValue::CreateBooleanValue(l.widgets.noteTop));wj.SetNamedValue(L"notePage",JsonValue::CreateNumberValue(l.widgets.notePage));wj.SetNamedValue(L"clockVisible",JsonValue::CreateBooleanValue(l.widgets.clockVisible));wj.SetNamedValue(L"clockX",JsonValue::CreateNumberValue(l.widgets.clockX));wj.SetNamedValue(L"clockY",JsonValue::CreateNumberValue(l.widgets.clockY));wj.SetNamedValue(L"clockW",JsonValue::CreateNumberValue(l.widgets.clockW));wj.SetNamedValue(L"clockH",JsonValue::CreateNumberValue(l.widgets.clockH));wj.SetNamedValue(L"noteMon",JsonValue::CreateStringValue(l.widgets.noteMon));wj.SetNamedValue(L"todoMon",JsonValue::CreateStringValue(l.widgets.todoMon));wj.SetNamedValue(L"clockMon",JsonValue::CreateStringValue(l.widgets.clockMon));wj.SetNamedValue(L"noteMX",JsonValue::CreateNumberValue(l.widgets.noteMX));wj.SetNamedValue(L"noteMY",JsonValue::CreateNumberValue(l.widgets.noteMY));wj.SetNamedValue(L"todoMX",JsonValue::CreateNumberValue(l.widgets.todoMX));wj.SetNamedValue(L"todoMY",JsonValue::CreateNumberValue(l.widgets.todoMY));wj.SetNamedValue(L"clockMX",JsonValue::CreateNumberValue(l.widgets.clockMX));wj.SetNamedValue(L"clockMY",JsonValue::CreateNumberValue(l.widgets.clockMY));wj.SetNamedValue(L"musicVisible",JsonValue::CreateBooleanValue(l.widgets.musicVisible));wj.SetNamedValue(L"musicX",JsonValue::CreateNumberValue(l.widgets.musicX));wj.SetNamedValue(L"musicY",JsonValue::CreateNumberValue(l.widgets.musicY));wj.SetNamedValue(L"musicW",JsonValue::CreateNumberValue(l.widgets.musicW));wj.SetNamedValue(L"musicH",JsonValue::CreateNumberValue(l.widgets.musicH));wj.SetNamedValue(L"weatherVisible",JsonValue::CreateBooleanValue(l.widgets.weatherVisible));wj.SetNamedValue(L"weatherX",JsonValue::CreateNumberValue(l.widgets.weatherX));wj.SetNamedValue(L"weatherY",JsonValue::CreateNumberValue(l.widgets.weatherY));wj.SetNamedValue(L"weatherW",JsonValue::CreateNumberValue(l.widgets.weatherW));wj.SetNamedValue(L"weatherH",JsonValue::CreateNumberValue(l.widgets.weatherH));wj.SetNamedValue(L"searchX",JsonValue::CreateNumberValue(l.widgets.searchX));wj.SetNamedValue(L"searchY",JsonValue::CreateNumberValue(l.widgets.searchY));wj.SetNamedValue(L"musicFolder",JsonValue::CreateStringValue(l.widgets.musicFolder));wj.SetNamedValue(L"musicIndex",JsonValue::CreateNumberValue(l.widgets.musicIndex));wj.SetNamedValue(L"musicVol",JsonValue::CreateNumberValue(l.widgets.musicVol));wj.SetNamedValue(L"weatherCity",JsonValue::CreateStringValue(l.widgets.weatherCity));wj.SetNamedValue(L"weatherLat",JsonValue::CreateNumberValue(l.widgets.weatherLat));wj.SetNamedValue(L"weatherLon",JsonValue::CreateNumberValue(l.widgets.weatherLon));wj.SetNamedValue(L"weatherSkin",JsonValue::CreateNumberValue(l.widgets.weatherSkin));wj.SetNamedValue(L"musicMon",JsonValue::CreateStringValue(l.widgets.musicMon));wj.SetNamedValue(L"weatherMon",JsonValue::CreateStringValue(l.widgets.weatherMon));wj.SetNamedValue(L"searchMon",JsonValue::CreateStringValue(l.widgets.searchMon));wj.SetNamedValue(L"musicMX",JsonValue::CreateNumberValue(l.widgets.musicMX));wj.SetNamedValue(L"musicMY",JsonValue::CreateNumberValue(l.widgets.musicMY));wj.SetNamedValue(L"weatherMX",JsonValue::CreateNumberValue(l.widgets.weatherMX));wj.SetNamedValue(L"weatherMY",JsonValue::CreateNumberValue(l.widgets.weatherMY));wj.SetNamedValue(L"searchMX",JsonValue::CreateNumberValue(l.widgets.searchMX));wj.SetNamedValue(L"searchMY",JsonValue::CreateNumberValue(l.widgets.searchMY));wj.SetNamedValue(L"appGridVisible",JsonValue::CreateBooleanValue(l.widgets.appGridVisible));wj.SetNamedValue(L"appGridX",JsonValue::CreateNumberValue(l.widgets.appGridX));wj.SetNamedValue(L"appGridY",JsonValue::CreateNumberValue(l.widgets.appGridY));wj.SetNamedValue(L"appGridW",JsonValue::CreateNumberValue(l.widgets.appGridW));wj.SetNamedValue(L"appGridH",JsonValue::CreateNumberValue(l.widgets.appGridH));wj.SetNamedValue(L"appGridMon",JsonValue::CreateStringValue(l.widgets.appGridMon));wj.SetNamedValue(L"appGridMX",JsonValue::CreateNumberValue(l.widgets.appGridMX));wj.SetNamedValue(L"appGridMY",JsonValue::CreateNumberValue(l.widgets.appGridMY));wj.SetNamedValue(L"clockBg",JsonValue::CreateStringValue(l.widgets.clockBg));wj.SetNamedValue(L"clockBgTrans",JsonValue::CreateNumberValue(l.widgets.clockBgTrans));JsonArray ps;for(auto const& p:l.widgets.pages){JsonObject pj;pj.SetNamedValue(L"text",JsonValue::CreateStringValue(p.text));pj.SetNamedValue(L"color",JsonValue::CreateNumberValue(p.color));ps.Append(pj);}wj.SetNamedValue(L"pages",ps);JsonArray ts;for(auto const& t:l.widgets.todos){JsonObject tj;tj.SetNamedValue(L"id",JsonValue::CreateStringValue(t.id));tj.SetNamedValue(L"text",JsonValue::CreateStringValue(t.text));tj.SetNamedValue(L"done",JsonValue::CreateBooleanValue(t.done));tj.SetNamedValue(L"due",JsonValue::CreateNumberValue(static_cast<double>(t.due)));tj.SetNamedValue(L"reminded",JsonValue::CreateBooleanValue(t.reminded));tj.SetNamedValue(L"flag",JsonValue::CreateNumberValue(t.flag));tj.SetNamedValue(L"repeat",JsonValue::CreateNumberValue(t.repeat));ts.Append(tj);}wj.SetNamedValue(L"todos",ts);root.SetNamedValue(L"widgets",wj);{JsonObject tp;tp.SetNamedValue(L"last",JsonValue::CreateStringValue(l.topologyLast));JsonObject ar;for(auto const& kv:l.topologyArchives)if(!kv.first.empty()&&kv.first.size()<=512&&!kv.second.empty()&&kv.second.size()<=262144)ar.SetNamedValue(kv.first,JsonValue::CreateStringValue(winrt::to_hstring(kv.second)));tp.SetNamedValue(L"archives",ar);root.SetNamedValue(L"topology",tp);}return to_string(root.Stringify());}
Layout Deserialize(std::string const& text){auto root=JsonObject::Parse(to_hstring(text)); if(root.GetNamedNumber(L"version")!=1)throw std::runtime_error("Unsupported layout version"); Layout l;std::vector<std::wstring> ids;auto number=[](JsonObject const& j,wchar_t const* key){double n=j.GetNamedNumber(key);if(!std::isfinite(n)||n < -100000 || n > 100000)throw std::runtime_error("Invalid geometry");return static_cast<int>(n);};for(auto const& value:root.GetNamedArray(L"zones")){auto j=value.GetObject();Zone z;z.id=j.GetNamedString(L"id");z.name=j.GetNamedString(L"name");if(z.id.empty()||std::find(ids.begin(),ids.end(),z.id)!=ids.end())throw std::runtime_error("Invalid zone identity");ids.push_back(z.id);z.x=number(j,L"x");z.y=number(j,L"y");z.width=number(j,L"width");z.height=number(j,L"height");if(z.width<=0||z.height<=0)throw std::runtime_error("Invalid size");z.collapsed=j.GetNamedBoolean(L"collapsed");if(j.HasKey(L"mappedFolder"))z.mappedFolder=std::wstring(j.GetNamedString(L"mappedFolder"));if(j.HasKey(L"viewMode")){z.viewMode=std::wstring(j.GetNamedString(L"viewMode"));if(z.viewMode!=L"grid"&&z.viewMode!=L"list")z.viewMode=L"grid";}if(j.HasKey(L"nameLines")){double n=j.GetNamedNumber(L"nameLines");z.nameLines=(n>=0&&n<=2&&n==static_cast<int>(n))?static_cast<int>(n):2;}if(j.HasKey(L"sortKey")){z.sortKey=std::wstring(j.GetNamedString(L"sortKey"));if(z.sortKey!=L"name"&&z.sortKey!=L"type"&&z.sortKey!=L"date"&&z.sortKey!=L"size")z.sortKey.clear();}if(j.HasKey(L"sortDescending"))z.sortDescending=j.GetNamedBoolean(L"sortDescending");if(j.HasKey(L"tileSize")){double n=j.GetNamedNumber(L"tileSize");z.tileSize=(n>=0&&n<=2&&n==static_cast<int>(n))?static_cast<int>(n):1;}if(j.HasKey(L"mon"))z.mon=std::wstring(j.GetNamedString(L"mon"));if(j.HasKey(L"mx")){double n=j.GetNamedNumber(L"mx");if(std::isfinite(n))z.mx=static_cast<int>(n);}if(j.HasKey(L"my")){double n=j.GetNamedNumber(L"my");if(std::isfinite(n))z.my=static_cast<int>(n);}if(j.HasKey(L"group"))z.group=std::wstring(j.GetNamedString(L"group"));if(j.HasKey(L"groupTab")){double n=j.GetNamedNumber(L"groupTab");if(std::isfinite(n)&&n>=0&&n==static_cast<int>(n))z.groupTab=static_cast<int>(n);}if(j.HasKey(L"capsule"))z.capsule=j.GetNamedBoolean(L"capsule");if(j.HasKey(L"browseInPlace"))z.browseInPlace=j.GetNamedBoolean(L"browseInPlace");if(j.HasKey(L"browseFolder"))z.browseFolder=std::wstring(j.GetNamedString(L"browseFolder"));if(j.HasKey(L"color")){double n=j.GetNamedNumber(L"color");z.color=(n>=0&&n<=8&&n==static_cast<int>(n))?static_cast<int>(n):0;}if(j.HasKey(L"background"))z.background=std::wstring(j.GetNamedString(L"background"));if(j.HasKey(L"dim")){double n=j.GetNamedNumber(L"dim");z.dim=(n>=0&&n<=2&&n==static_cast<int>(n))?static_cast<int>(n):1;}if(j.HasKey(L"pins")){for(auto const& pv:j.GetNamedArray(L"pins")){if(z.pins.size()>=12)break;try{auto p=std::wstring(pv.GetString());if(p.empty()||p.size()>260)continue;auto pk=PathKey(p);if(std::any_of(z.pins.begin(),z.pins.end(),[&](auto const& q){return PathKey(q)==pk;}))continue;z.pins.push_back(p);}catch(...){}}}if(j.HasKey(L"opacity")){try{z.opacity=ClampOpacityPercent(static_cast<int>(j.GetNamedNumber(L"opacity")));}catch(...){z.opacity=100;}}if(j.HasKey(L"expandDir")){try{z.expandDir=ClampExpandDir(static_cast<int>(j.GetNamedNumber(L"expandDir")));}catch(...){z.expandDir=0;}}if(j.HasKey(L"maxHeight")){try{z.maxHeight=ClampMaxHeight(static_cast<int>(j.GetNamedNumber(L"maxHeight")));}catch(...){z.maxHeight=0;}}if(j.HasKey(L"locked")){try{z.locked=j.GetNamedBoolean(L"locked");}catch(...){z.locked=false;}}if(j.HasKey(L"autoArchive")){try{z.autoArchive=ClampArchiveMode(static_cast<int>(j.GetNamedNumber(L"autoArchive")));}catch(...){z.autoArchive=0;}}if(j.HasKey(L"archiveAt")){try{double n=j.GetNamedNumber(L"archiveAt");z.archiveAt=(std::isfinite(n)&&n>0&&n<4102444800.0)?static_cast<long long>(n):0;}catch(...){z.archiveAt=0;}}if(j.HasKey(L"stackGrid")){try{z.stackGrid=ClampStackGrid(static_cast<int>(j.GetNamedNumber(L"stackGrid")));}catch(...){z.stackGrid=0;}}if(j.HasKey(L"stacks")){for(auto const& sv:j.GetNamedArray(L"stacks")){try{auto sj=sv.GetObject();Stack st;st.id=sj.GetNamedString(L"id");st.name=sj.GetNamedString(L"name");if(!st.id.empty()&&!st.name.empty()&&std::find_if(z.stacks.begin(),z.stacks.end(),[&](Stack const& x){return x.id==st.id;})==z.stacks.end())z.stacks.push_back(std::move(st));}catch(...){}}}for(auto const& ev:j.GetNamedArray(L"entries")){auto ej=ev.GetObject();std::wstring p(ej.GetNamedString(L"path")),id(ej.GetNamedString(L"id"));if(p.empty()||!std::filesystem::path(p).is_absolute()||id.empty()||std::find(ids.begin(),ids.end(),id)!=ids.end())throw std::runtime_error("Invalid entry");ids.push_back(id);if(AddEntry(z,p)){z.entries.back().id=id;if(ej.HasKey(L"stack"))z.entries.back().stack=std::wstring(ej.GetNamedString(L"stack"));}}l.zones.push_back(std::move(z));}if(root.HasKey(L"settings")){auto s=root.GetNamedObject(L"settings");if(s.HasKey(L"theme"))l.settings.theme=std::wstring(s.GetNamedString(L"theme"));if(s.HasKey(L"compact"))l.settings.compact=s.GetNamedBoolean(L"compact");if(s.HasKey(L"language"))l.settings.language=std::wstring(s.GetNamedString(L"language"));if(s.HasKey(L"hotkey")){l.settings.hotkey=std::wstring(s.GetNamedString(L"hotkey"));Hotkey hk;if(!IsDoubleCtrlHotkey(l.settings.hotkey)&&!ParseHotkey(l.settings.hotkey,hk))l.settings.hotkey.clear();}if(s.HasKey(L"snapshots"))l.settings.snapshots=s.GetNamedBoolean(L"snapshots");if(s.HasKey(L"guideDone"))l.settings.guideDone=s.GetNamedBoolean(L"guideDone");if(s.HasKey(L"hotkeySearch")){l.settings.hotkeySearch=std::wstring(s.GetNamedString(L"hotkeySearch"));Hotkey hks;if(!l.settings.hotkeySearch.empty()&&!ParseHotkey(l.settings.hotkeySearch,hks))l.settings.hotkeySearch.clear();}if(s.HasKey(L"hotkeyCapture")){l.settings.hotkeyCapture=std::wstring(s.GetNamedString(L"hotkeyCapture"));Hotkey hkc;if(!l.settings.hotkeyCapture.empty()&&!ParseHotkey(l.settings.hotkeyCapture,hkc))l.settings.hotkeyCapture.clear();}if(s.HasKey(L"performance"))l.settings.performance=s.GetNamedBoolean(L"performance");if(s.HasKey(L"syncUrl")){l.settings.syncUrl=std::wstring(s.GetNamedString(L"syncUrl"));webdav::UrlParts up;if(!l.settings.syncUrl.empty()&&!webdav::ParseUrl(l.settings.syncUrl,up))l.settings.syncUrl.clear();}if(s.HasKey(L"syncUser"))l.settings.syncUser=std::wstring(s.GetNamedString(L"syncUser"));if(s.HasKey(L"syncPass"))l.settings.syncPass=std::wstring(s.GetNamedString(L"syncPass"));if(s.HasKey(L"syncAuto"))l.settings.syncAuto=s.GetNamedBoolean(L"syncAuto");if(s.HasKey(L"syncInsecure"))l.settings.syncInsecure=s.GetNamedBoolean(L"syncInsecure");if(s.HasKey(L"textSize")){double n=s.GetNamedNumber(L"textSize");l.settings.textSize=(n>=0&&n<=2&&n==static_cast<int>(n))?static_cast<int>(n):1;}if(s.HasKey(L"clockStyle")){l.settings.clockStyle=std::wstring(s.GetNamedString(L"clockStyle"));if(l.settings.clockStyle!=L"analog")l.settings.clockStyle.clear();}if(s.HasKey(L"everything"))l.settings.everything=s.GetNamedBoolean(L"everything");if(s.HasKey(L"hotkeyUndo")){l.settings.hotkeyUndo=std::wstring(s.GetNamedString(L"hotkeyUndo"));Hotkey hku;if(!l.settings.hotkeyUndo.empty()&&!ParseHotkey(l.settings.hotkeyUndo,hku))l.settings.hotkeyUndo.clear();}if(s.HasKey(L"tabHover"))l.settings.tabHover=s.GetNamedBoolean(L"tabHover");if(s.HasKey(L"memTrim"))l.settings.memTrim=s.GetNamedBoolean(L"memTrim");if(s.HasKey(L"revealHotkey")){l.settings.revealHotkey=std::wstring(s.GetNamedString(L"revealHotkey"));Hotkey hkr;if(!l.settings.revealHotkey.empty()&&!ParseHotkey(l.settings.revealHotkey,hkr))l.settings.revealHotkey.clear();}if(s.HasKey(L"hotkeyRaise")){l.settings.hotkeyRaise=std::wstring(s.GetNamedString(L"hotkeyRaise"));Hotkey hkg;if(!l.settings.hotkeyRaise.empty()&&!ParseHotkey(l.settings.hotkeyRaise,hkg))l.settings.hotkeyRaise.clear();}if(s.HasKey(L"backdrop")){try{l.settings.backdrop=ClampBackdropKind(static_cast<int>(s.GetNamedNumber(L"backdrop")));}catch(...){l.settings.backdrop=0;}}auto strArr=[&](wchar_t const* key,std::vector<std::wstring>& target){if(!s.HasKey(key))return;try{for(auto const& v:s.GetNamedArray(key)){try{std::wstring q=std::wstring(v.GetString());if(!q.empty()&&q.size()<=200&&target.size()<100)target.push_back(q);}catch(...){}}}catch(...){}};strArr(L"searchHistory",l.settings.searchHistory);strArr(L"searchFavorites",l.settings.searchFavorites);}if(root.HasKey(L"rules")){for(auto const& value:root.GetNamedArray(L"rules")){try{auto rj=value.GetObject();Rule r;r.id=rj.GetNamedString(L"id");r.name=rj.GetNamedString(L"name");if(r.id.empty())continue;if(r.name.empty())r.name=i18n::Tr(L"未命名规则");for(auto const& e:rj.GetNamedArray(L"exts"))r.exts.push_back(std::wstring(e.GetString()));for(auto const& k:rj.GetNamedArray(L"keywords"))r.keywords.push_back(std::wstring(k.GetString()));if(rj.HasKey(L"zone"))r.targetZone=std::wstring(rj.GetNamedString(L"zone"));
auto gate=[&](wchar_t const* key,long long cap){double n=0;try{if(rj.HasKey(key))n=rj.GetNamedNumber(key);}catch(...){n=0;}if(!std::isfinite(n)||n<0)n=0;if(n>static_cast<double>(cap))n=static_cast<double>(cap);return static_cast<long long>(n);};
r.minSizeKb=ClampSizeKb(gate(L"minSize",100000000LL));
r.maxSizeKb=ClampSizeKb(gate(L"maxSize",100000000LL));
r.olderThanDays=ClampAgeDays(static_cast<int>(gate(L"olderThan",36500LL)));
// 手写 layout.json 里把上下限填反会让规则永远命中不了，按 UI 的约定交换回来
if(r.minSizeKb>0&&r.maxSizeKb>0&&r.maxSizeKb<r.minSizeKb)std::swap(r.minSizeKb,r.maxSizeKb);
l.rules.push_back(std::move(r));}catch(...){}}}if(l.rules.empty())DefaultRules(l);if(root.HasKey(L"widgets")){try{auto wj=root.GetNamedObject(L"widgets");auto& w=l.widgets;if(wj.HasKey(L"noteVisible"))w.noteVisible=wj.GetNamedBoolean(L"noteVisible");if(wj.HasKey(L"todoVisible"))w.todoVisible=wj.GetNamedBoolean(L"todoVisible");if(wj.HasKey(L"clockVisible"))w.clockVisible=wj.GetNamedBoolean(L"clockVisible");if(wj.HasKey(L"musicVisible"))w.musicVisible=wj.GetNamedBoolean(L"musicVisible");if(wj.HasKey(L"weatherVisible"))w.weatherVisible=wj.GetNamedBoolean(L"weatherVisible");if(wj.HasKey(L"appGridVisible"))w.appGridVisible=wj.GetNamedBoolean(L"appGridVisible");if(wj.HasKey(L"noteText"))w.noteText=std::wstring(wj.GetNamedString(L"noteText"));auto num=[&](wchar_t const* k,int def)->int{if(!wj.HasKey(k))return def;double n=wj.GetNamedNumber(k);return std::isfinite(n)?static_cast<int>(n):def;};w.noteX=num(L"noteX",w.noteX);w.noteY=num(L"noteY",w.noteY);w.noteW=num(L"noteW",w.noteW);w.noteH=num(L"noteH",w.noteH);w.todoX=num(L"todoX",w.todoX);w.todoY=num(L"todoY",w.todoY);w.todoW=num(L"todoW",w.todoW);w.todoH=num(L"todoH",w.todoH);w.clockX=num(L"clockX",w.clockX);w.clockY=num(L"clockY",w.clockY);w.clockW=num(L"clockW",w.clockW);w.clockH=num(L"clockH",w.clockH);if(w.noteW<160)w.noteW=160;if(w.noteH<120)w.noteH=120;if(w.todoW<220)w.todoW=220;if(w.todoH<200)w.todoH=200;if(w.clockW<150)w.clockW=150;if(w.clockH<110)w.clockH=110;w.musicX=num(L"musicX",w.musicX);w.musicY=num(L"musicY",w.musicY);w.musicW=num(L"musicW",w.musicW);w.musicH=num(L"musicH",w.musicH);w.weatherX=num(L"weatherX",w.weatherX);w.weatherY=num(L"weatherY",w.weatherY);w.weatherW=num(L"weatherW",w.weatherW);w.weatherH=num(L"weatherH",w.weatherH);w.searchX=num(L"searchX",w.searchX);w.searchY=num(L"searchY",w.searchY);w.appGridX=num(L"appGridX",w.appGridX);w.appGridY=num(L"appGridY",w.appGridY);w.appGridW=num(L"appGridW",w.appGridW);w.appGridH=num(L"appGridH",w.appGridH);if(w.appGridW<260)w.appGridW=260;if(w.appGridH<280)w.appGridH=280;if(w.musicW<240)w.musicW=240;if(w.musicH<300)w.musicH=300;if(w.weatherW<220)w.weatherW=220;if(w.weatherH<280)w.weatherH=280;w.musicIndex=num(L"musicIndex",w.musicIndex);if(w.musicIndex<0)w.musicIndex=0;w.musicVol=num(L"musicVol",w.musicVol);w.musicVol=std::clamp(w.musicVol,0,100);if(wj.HasKey(L"noteTop"))w.noteTop=wj.GetNamedBoolean(L"noteTop");if(wj.HasKey(L"notePage")){double n=wj.GetNamedNumber(L"notePage");w.notePage=(std::isfinite(n)&&n>=0)?static_cast<int>(n):0;}auto mon=[&](wchar_t const* k,std::wstring& target){if(wj.HasKey(k))target=std::wstring(wj.GetNamedString(k));};mon(L"noteMon",w.noteMon);mon(L"todoMon",w.todoMon);mon(L"clockMon",w.clockMon);mon(L"musicMon",w.musicMon);mon(L"weatherMon",w.weatherMon);mon(L"searchMon",w.searchMon);mon(L"appGridMon",w.appGridMon);auto num2=[&](wchar_t const* k,int& target){if(wj.HasKey(k)){double n=wj.GetNamedNumber(k);if(std::isfinite(n))target=static_cast<int>(n);}};num2(L"noteMX",w.noteMX);num2(L"noteMY",w.noteMY);num2(L"todoMX",w.todoMX);num2(L"todoMY",w.todoMY);num2(L"clockMX",w.clockMX);num2(L"clockMY",w.clockMY);num2(L"musicMX",w.musicMX);num2(L"musicMY",w.musicMY);num2(L"weatherMX",w.weatherMX);num2(L"weatherMY",w.weatherMY);num2(L"searchMX",w.searchMX);num2(L"searchMY",w.searchMY);num2(L"appGridMX",w.appGridMX);num2(L"appGridMY",w.appGridMY);if(wj.HasKey(L"musicFolder"))w.musicFolder=std::wstring(wj.GetNamedString(L"musicFolder"));if(wj.HasKey(L"weatherCity"))w.weatherCity=std::wstring(wj.GetNamedString(L"weatherCity"));auto dnum=[&](wchar_t const* k,double& target){if(!wj.HasKey(k))return;double n=wj.GetNamedNumber(k);if(std::isfinite(n))target=n;};dnum(L"weatherLat",w.weatherLat);dnum(L"weatherLon",w.weatherLon);if(w.weatherLat<-90||w.weatherLat>90||w.weatherLon<-180||w.weatherLon>180){w.weatherLat=999;w.weatherLon=999;}if(wj.HasKey(L"weatherSkin")){double n=wj.GetNamedNumber(L"weatherSkin");w.weatherSkin=(n>=0&&n<=1&&n==static_cast<int>(n))?static_cast<int>(n):0;}if(wj.HasKey(L"clockBg"))w.clockBg=std::wstring(wj.GetNamedString(L"clockBg"));if(wj.HasKey(L"clockBgTrans")){try{w.clockBgTrans=ClampOpacityPercent(static_cast<int>(wj.GetNamedNumber(L"clockBgTrans")));}catch(...){w.clockBgTrans=100;}}if(wj.HasKey(L"pages")){for(auto const& value:wj.GetNamedArray(L"pages")){try{auto pj=value.GetObject();NotePage p;p.text=std::wstring(pj.GetNamedString(L"text"));if(pj.HasKey(L"color")){double n=pj.GetNamedNumber(L"color");p.color=(n>=0&&n<=5&&n==static_cast<int>(n))?static_cast<int>(n):0;}w.pages.push_back(std::move(p));}catch(...){}}}if(w.pages.empty()&&!w.noteText.empty())w.pages.push_back({w.noteText,0});if(wj.HasKey(L"todos")){for(auto const& value:wj.GetNamedArray(L"todos")){try{auto tj=value.GetObject();TodoItem t;t.id=tj.GetNamedString(L"id");t.text=tj.GetNamedString(L"text");if(t.id.empty()||t.text.empty())continue;t.done=tj.HasKey(L"done")&&tj.GetNamedBoolean(L"done");if(tj.HasKey(L"due")){double n=tj.GetNamedNumber(L"due");if(std::isfinite(n)&&n>0)t.due=static_cast<long long>(n);}if(tj.HasKey(L"reminded"))t.reminded=tj.GetNamedBoolean(L"reminded");if(tj.HasKey(L"flag")){try{int f=static_cast<int>(tj.GetNamedNumber(L"flag"));t.flag=(f>=1&&f<=3)?f:0;}catch(...){t.flag=0;}}if(tj.HasKey(L"repeat")){try{int r=static_cast<int>(tj.GetNamedNumber(L"repeat"));t.repeat=(r>=1&&r<=4)?r:0;}catch(...){t.repeat=0;}}w.todos.push_back(std::move(t));}catch(...){}}}}catch(...){}}if(root.HasKey(L"topology")){try{auto tj=root.GetNamedObject(L"topology");if(tj.HasKey(L"last")){std::wstring s=std::wstring(tj.GetNamedString(L"last"));if(!s.empty()&&s.size()<=512)l.topologyLast=s;}if(tj.HasKey(L"archives")){for(auto const& kv:tj.GetNamedObject(L"archives")){try{std::wstring k(kv.Key());std::string b=winrt::to_string(kv.Value().GetString());if(k.empty()||k.size()>512||b.empty()||b.size()>262144||l.topologyArchives.size()>=6)continue;l.topologyArchives[k]=std::move(b);}catch(...){}}}}catch(...){}}for(auto& z:l.zones){if(!z.mappedFolder.empty()&&!std::filesystem::path(z.mappedFolder).is_absolute())z.mappedFolder.clear();if(z.mappedFolder.empty()||!z.browseInPlace||!UnderRoot(z.mappedFolder,z.browseFolder))z.browseFolder.clear();if(!IsImagePath(z.background))z.background.clear();if(z.group.empty())continue;auto members=GroupMemberIds(l,z.group);if(members.size()<2){z.group.clear();z.groupTab=0;continue;}z.groupTab=std::clamp(z.groupTab,0,static_cast<int>(members.size())-1);}for(auto& z:l.zones)for(auto& e:z.entries)if(!e.stack.empty()&&std::find_if(z.stacks.begin(),z.stacks.end(),[&](Stack const& s){return s.id==e.stack;})==z.stacks.end())e.stack.clear();return l;}
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

