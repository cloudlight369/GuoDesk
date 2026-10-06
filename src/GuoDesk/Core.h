#pragma once
#include <map>
namespace guodesk {
struct Entry { std::wstring id, path; std::wstring stack; };
struct Stack { std::wstring id, name; };
struct Zone { std::wstring id, name=L"常用"; int x=120,y=120,width=440,height=360; bool collapsed=false; std::vector<Entry> entries; std::wstring mappedFolder; std::wstring viewMode=L"grid"; int nameLines=2; std::wstring sortKey; bool sortDescending=false; int tileSize=1; std::wstring mon; int mx=0,my=0; std::vector<Stack> stacks; std::wstring group; int groupTab=0; bool capsule=false; bool browseInPlace=true; std::wstring browseFolder; int color=0; std::wstring background; int dim=1; std::vector<std::wstring> pins; int opacity=100; int expandDir=0; int maxHeight=0; bool locked=false; int autoArchive=0; long long archiveAt=0; int stackGrid=0; };
struct Settings { std::wstring theme; bool compact=false; std::wstring language; std::wstring hotkey=L"Ctrl+Alt+G"; bool snapshots=true; bool guideDone=false; std::wstring hotkeySearch; std::wstring hotkeyCapture; std::wstring hotkeyUndo=L"Ctrl+Alt+U"; int perfTier=0; int labelStyle=0; std::wstring syncUrl,syncUser,syncPass; bool syncAuto=false; bool syncInsecure=false; int textSize=1; std::wstring clockStyle; bool everything=true; bool tabHover=true; bool memTrim=true; std::wstring revealHotkey,hotkeyRaise; int backdrop=0; std::vector<std::wstring> searchHistory,searchFavorites; };
struct Rule { std::wstring id, name; std::vector<std::wstring> exts, keywords; std::wstring targetZone; long long minSizeKb=0, maxSizeKb=0; int olderThanDays=0; };
struct TodoItem { std::wstring id, text; bool done=false; long long due=0; bool reminded=false; int flag=0; int repeat=0; };
struct NotePage { std::wstring text; int color=0; };
struct Widgets {
 bool noteVisible=false,todoVisible=false,clockVisible=false,noteTop=false,musicVisible=false,weatherVisible=false,appGridVisible=false;
 std::wstring noteText;
 int noteX=340,noteY=180,noteW=300,noteH=240; int notePage=0;
 int todoX=680,todoY=180,todoW=300,todoH=420;
 int clockX=980,clockY=60,clockW=220,clockH=150;
 std::wstring clockBg; int clockBgTrans=100;
 int musicX=980,musicY=280,musicW=280,musicH=380;
 int weatherX=660,weatherY=520,weatherW=250,weatherH=340;
 int appGridX=340,appGridY=520,appGridW=340,appGridH=400;
 int searchX=560,searchY=260;
 std::wstring musicFolder; int musicIndex=0; int musicVol=65;
 std::wstring weatherCity; double weatherLat=999,weatherLon=999; int weatherSkin=0;
 std::wstring noteMon,todoMon,clockMon,musicMon,weatherMon,searchMon,appGridMon; int noteMX=0,noteMY=0,todoMX=0,todoMY=0,clockMX=0,clockMY=0,musicMX=0,musicMY=0,weatherMX=0,weatherMY=0,searchMX=0,searchMY=0,appGridMX=0,appGridMY=0;
 std::vector<NotePage> pages;
 std::vector<TodoItem> todos;
};
struct Layout { std::vector<Zone> zones; Settings settings; std::vector<Rule> rules; Widgets widgets; std::wstring topologyLast; std::map<std::wstring,std::string> topologyArchives; };
struct UndoFrame { std::wstring label; std::string snapshot; };
class UndoStack {
 std::vector<UndoFrame> frames; size_t limit;
public:
 explicit UndoStack(size_t limit=20);
 void Push(std::wstring label,std::string snapshot);
 bool Pop(UndoFrame& out);
 bool Empty() const;
 size_t Count() const;
 size_t Limit() const;
 std::wstring const& TopLabel() const;
 void Clear();
};
struct TemplateZone { std::wstring name, folderTag; int rx=0,ry=0,rw=494,rh=494; };
struct ZoneTemplate { std::wstring id, name; std::vector<TemplateZone> zones; };
struct PlanItem { std::wstring path, rule, zone; };
struct SearchHit { std::wstring path, name, zone, kind; };
struct WeatherDay { int code=0; double lo=0,hi=0; long long date=0; };
struct WeatherHour { long long key=0; int code=0; double temp=0; };
struct WeatherNow { bool valid=false; double temp=0,hi=0,lo=0; int humidity=0; int code=0; bool isDay=true; std::wstring city; std::vector<WeatherDay> days; std::vector<WeatherHour> hours; };
struct GeoPlace { std::wstring name,country; double lat=0,lon=0; };
enum class SortKey { Name, Type, Date, Size };
struct Hotkey { unsigned mods=0, vk=0; };
struct MonitorArea { std::wstring device; RECT work; };
SortKey SortKeyFromString(std::wstring const& value);
bool ParseHotkey(std::wstring const& text,Hotkey& out);
std::wstring HotkeyToString(Hotkey const& hotkey);
void SortEntries(Zone& zone,SortKey key,bool descending);
std::vector<MonitorArea> EnumMonitorAreas();
void Reanchor(RECT& rect,std::wstring& mon,int& mx,int& my);
std::wstring TopologySignature(std::vector<MonitorArea> const& areas);
bool ArchiveTopology(std::map<std::wstring,std::string>& archives,std::wstring const& key,std::string const& body,size_t maxArchives=6,size_t maxBytes=262144);
bool TakeTopology(std::map<std::wstring,std::string> const& archives,std::wstring const& key,std::string& out);
long long DueFromOffset(int days);
double ScaledFont(int textSize,double base);
struct LunarDate { int year=0,month=0,day=0; bool leap=false; bool valid=false; };
LunarDate LunarFromSolar(int year,int month,int day);
std::wstring LunarText(LunarDate const& date);
std::wstring HolidayText(int year,int month,int day,LunarDate const& lunar);
int ZoneColorCount();
unsigned ZoneColorRGB(int color);
int ClampOpacityPercent(int v);
int NextOpacityStep(int v);
int ClampBackdropKind(int v);
int ClampExpandDir(int v);
int ClampMaxHeight(int v);
int ZoneCapHeight(int maxHeight,int workHeight);
int ZoneExpandedHeight(int height,int maxHeight,int workHeight);
// 在渲染出来的条目里找一个路径（大小写/短路径无关），返回下标，找不到返回 -1
int NavIndexOf(std::vector<std::wstring> const& paths,std::wstring const& want);
int ZoneAnchorTop(int top,int height,int targetHeight,int dir,int workTop,int workBottom);
wchar_t const* ExpandDirName(int dir);
std::wstring MediaTrackLine(std::wstring const& title,std::wstring const& artist);
std::wstring MediaTimeText(long long posSec,long long endSec);
int MediaStatusKind(int raw);
bool IsImagePath(std::wstring const& path);
std::vector<std::wstring> FilterImagePaths(std::vector<std::wstring> const& paths);
bool IsTextPath(std::wstring const& path);
int PreviewKind(std::wstring const& path);
std::wstring DecodeNeutralText(std::vector<char> const& raw);
std::wstring ClampPreviewText(std::wstring const& text,size_t maxChars=20000);
std::wstring PreviewSizeText(long long bytes);
int ProgressPercent(long long done,long long total);
int OpOutcome(long long ok,long long failed,bool cancelled);
bool IsReservedDeviceName(std::wstring const& name);
// 拖入落盘语义：0=按引用加入口（普通分区），1=复制到映射文件夹，2=移动到映射文件夹
int DropOperation(bool mappedZone,bool shiftPressed);
std::wstring BuildDiagnostics(Layout const& l,std::wstring const& version,std::wstring const& machine,std::wstring const& osBuild,long long today);
std::wstring DueText(long long due);
bool DueReached(long long due);
void SetTodoDue(Widgets& widgets,std::wstring const& id,long long due);
int TodoFlagCount();
unsigned TodoFlagRGB(int flag);
bool TodoMatchesFilter(TodoItem const& t,int filter);
int ClearDoneTodos(Widgets& widgets);
struct AppShortcut { std::wstring name, linkPath, target, arguments; };
bool ShouldListApp(std::wstring const& name,std::wstring const& target);
bool AppMatches(std::wstring const& name,std::wstring const& query);
void SortAppsByName(std::vector<AppShortcut>& apps);
NotePage* ActiveNote(Widgets& widgets);
void AddNotePage(Widgets& widgets);
void RemoveNotePage(Widgets& widgets);
bool AppendNote(Widgets& widgets,std::wstring const& text);
std::wstring NewId();
std::wstring PathKey(std::wstring const& path);
bool AddEntry(Zone& zone,std::wstring const& path);
std::vector<Entry> ListMapped(std::wstring const& folder);
void SyncMapped(Zone& zone);
bool UnderRoot(std::wstring const& root,std::wstring const& path);
bool SelfNesting(std::wstring const& source,std::wstring const& destDir);
std::vector<std::wstring> Crumbs(std::wstring const& root,std::wstring const& current);
std::wstring CrumbParent(std::wstring const& root,std::wstring const& current);
void Clamp(Zone& zone,RECT const& area);
void DefaultRules(Layout& layout);
long long ClampSizeKb(long long value);
int ClampAgeDays(int value);
// sizeKb/ageDays 传负数表示"取不到"：此时带尺寸或时间门槛的规则一律不命中，避免把没法度量的文件塞进它够格的分类
bool RuleMatches(Rule const& rule,std::wstring const& ext,std::wstring const& lowerName,long long sizeKb,long long ageDays);
std::wstring CategoryFolder(std::wstring const& name,std::wstring const& fallback);
struct ArchiveGroup { std::wstring category; std::vector<std::wstring> paths; };
std::vector<ArchiveGroup> ArchivePlan(std::vector<Rule> const& rules,std::vector<std::wstring> const& files,std::vector<std::wstring>* unmatched,std::wstring const& root={});
// 规则按分区生效：未绑定分区的规则对哪个分区都算，绑定过的只在它自己的分区里算
std::vector<Rule> RulesForZone(std::vector<Rule> const& rules,std::wstring const& zoneId);
// 数一数有多少条规则绑在这个分区上：分区被删后它们谁也不作用于，但绝不能被悄悄放大成全局
int CountRulesBoundTo(std::vector<Rule> const& rules,std::wstring const& zoneId);
// 分区还在不在（规则绑定是不是已经悬空）
bool HasZone(std::vector<Zone> const& zones,std::wstring const& zoneId);
// 自动归档的排程：mode 0=关闭 1=每小时 2=每天，lastRun=0 表示从未跑过（立刻该跑）
long long NowEpoch();
long long ArchiveIntervalSeconds(int mode);
bool ArchiveDue(int mode,long long lastRun,long long now);
int ClampArchiveMode(int value);
// 叠放缩略图：0=只显示首项，1/2/3=3×3、4×4、5×5 宫格
int ClampStackGrid(int value);
int StackCells(int mode);
// 宫格画得下几列：按磁贴里真正可用的像素高度算，每格至少 12px（含间隙），1 表示退回单图
int StackSide(int availPx,int mode);
// 性能三档：0=完整特效 1=精简（不播入场动画）2=省电（再关掉背景材质与叠放宫格）
int ClampPerfTier(int value);
bool PerfAnim(int tier);
bool PerfMaterial(int tier);
bool PerfMosaic(int tier);
int PerfTierFromLegacy(bool performance);
// 条目名称底板：0=不加（跟随主题）1=半透明黑底 2=高对比不透明底，花壁纸下保证看得清
int ClampLabelStyle(int value);
// 名称占位高度（像素）：按字号算行高，再把底板 Border 的上下 padding 一起算进去，磁贴和叠放宫格都按这个数留地方
int LabelHeightPx(int nameLines,int textSizeTier,int chromePadding);
// 底板浓度（alpha）：0=不垫 1=半透明 2=高对比，条目名的小药丸和顶/底栏的面板共用这一档
int LabelAlpha(int style);
// URL 拖入下载：只接受 http/https，文件名从路径尾部取（百分号解码+非法字符清洗），取不到就用 fallback
bool IsHttpUrl(std::wstring const& url);
// 托盘气泡的定长缓冲：超长直接触发 wcscpy_s 的无效参数处理器，整个进程会被终止，所以入口一律裁切
std::wstring ClipBalloon(std::wstring const& text,size_t capacity);
std::wstring DownloadName(std::wstring const& url,std::wstring const& contentType={});
std::wstring ExtFromContentType(std::wstring const& contentType);
std::vector<std::wstring> ListLooseFiles(std::wstring const& folder,bool recursive=false,size_t limit=2000);
std::vector<std::wstring> DesktopFileList();
std::vector<PlanItem> BuildPlan(std::vector<Rule> const& rules,std::vector<Zone> const& zones,std::vector<std::wstring> const& files,std::vector<std::wstring>* unmatched);
int ApplyPlan(Layout& layout,std::vector<PlanItem> const& plan);
std::wstring KnownFolder(std::wstring const& tag);
std::wstring KnownFolderName(std::wstring const& tag);
std::vector<ZoneTemplate> BuiltInTemplates();
int ApplyTemplate(Layout& layout,ZoneTemplate const& tpl,RECT const& work);
int AddQuickZone(Layout& layout,std::wstring const& tag,RECT const& work);
TodoItem* AddTodo(Widgets& widgets,std::wstring const& text);
long long NextDue(long long due,int repeat,long long today);
int ToggleTodo(Widgets& widgets,std::wstring const& id);
void RemoveTodo(Widgets& widgets,std::wstring const& id);
struct MdSeg { std::wstring text; int style=0; };
std::vector<MdSeg> ParseInlineMarkdown(std::wstring const& s);
bool TodoMatchesQuery(TodoItem const& t,std::wstring const& q);
int RemoveTodos(Widgets& widgets,std::vector<std::wstring> const& ids);
std::wstring NewStackName(Zone const& zone);
std::wstring CreateStack(Zone& zone);
void AssignStack(Zone& zone,std::wstring const& entryId,std::wstring const& stackId);
int StackCount(Zone const& zone,std::wstring const& stackId);
void DissolveStack(Zone& zone,std::wstring const& stackId);
int MoveStack(Layout& layout,std::wstring const& stackId,std::wstring const& fromZone,std::wstring const& toZone);
std::vector<std::wstring> GroupMemberIds(Layout const& layout,std::wstring const& group);
bool IsMusicFile(std::wstring const& path);
std::vector<std::wstring> MusicPlaylist(std::wstring const& folder);
bool SearchMatch(std::wstring const& text,std::wstring const& query);
void SearchZones(Layout const& layout,std::wstring const& query,std::vector<SearchHit>& out);
void SearchWidgets(Layout const& layout,std::wstring const& query,std::vector<SearchHit>& out);
inline bool IsDoubleCtrlHotkey(std::wstring const& text){return text==L"DoubleCtrl";}
void PushSearchHistory(std::vector<std::wstring>& history,std::wstring const& query,size_t limit=20);
bool ToggleSearchFavorite(std::vector<std::wstring>& favorites,std::wstring const& query,size_t limit=50);
bool ToggleSelect(std::vector<std::wstring>& keys,std::wstring const& key,size_t limit=50);
int SelectRange(std::vector<std::wstring>& keys,std::vector<std::wstring> const& paths,int anchor,int target,size_t limit=50);
std::wstring UniqueName(std::vector<std::wstring> const& taken,std::wstring const& base,std::wstring const& ext=L"");
int NavStep(int cur,int count,int delta);
std::wstring WmoText(int code);
std::wstring WmoEmoji(int code);
int WeatherSkinCount();
unsigned WeatherSkinTop(int skin);
unsigned WeatherSkinBottom(int skin);
int WeatherSkinForCode(int code,bool night);
long long NowHourKey();
WeatherNow ParseWeatherJson(std::wstring const& json,long long nowKey=0);
std::vector<GeoPlace> ParseGeoJson(std::wstring const& json);
GeoPlace ParseIpLocJson(std::wstring const& json);
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
