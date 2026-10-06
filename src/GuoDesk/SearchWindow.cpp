#include "pch.h"
#include "SearchWindow.h"
#include "DeskWindow.h"
#include "EvSearch.h"
#include "Shell.h"
#include "I18n.h"
#include <thread>
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
namespace guodesk {
static std::wstring SearchExePath(){wchar_t buf[MAX_PATH]{};GetModuleFileNameW(nullptr,buf,MAX_PATH);return buf;}
static Brush ResolveSearchBrush(wchar_t const* key,Windows::UI::Color fallback,bool dark){
 try{return Application::Current().Resources().Lookup(box_value(key)).as<Brush>();}catch(...){}
 struct Entry{wchar_t const* key;Windows::UI::Color light;Windows::UI::Color dark;};
 static const Entry palette[]{{L"TextFillColorSecondary",{255,97,97,97},{255,199,199,199}}};
 for(auto const& e:palette)if(!wcscmp(e.key,key))return SolidColorBrush(dark?e.dark:e.light);
 return SolidColorBrush(fallback);
}
Brush SearchWindow::ThemeBrush(wchar_t const* key,Windows::UI::Color fallback){bool dark=false;try{dark=root.ActualTheme()==ElementTheme::Dark;}catch(...){}return ResolveSearchBrush(key,fallback,dark);}
static std::wstring WebUrl(std::wstring const& q){
 std::string utf8=winrt::to_string(q);
 std::wstring enc;
 for(unsigned char c:utf8){
  if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')enc+=static_cast<wchar_t>(c);
  else{wchar_t b[4]{};swprintf_s(b,4,L"%%%02X",c);enc+=b;}
 }
 return L"https://www.bing.com/search?q="+enc;
}
void SearchWindow::OpenPath(std::wstring const& path){try{shell::Open(hwnd,path);}catch(...){try{hint.Text(i18n::Tr(L"无法打开目标，它可能已被移动或删除。"));}catch(...){}}}
void SearchWindow::NoteQuery(){
 auto q=std::wstring(query.Text());
 auto& st=owner.layout.settings;
 auto before=st.searchHistory;
 PushSearchHistory(st.searchHistory,q);
 if(st.searchHistory!=before)owner.Save();
}
void SearchWindow::UpdateFavGlyph(std::wstring const& q){
 if(!favToggle)return;
 auto const& sf=owner.layout.settings.searchFavorites;
 bool on=std::find(sf.begin(),sf.end(),q)!=sf.end();
 FontIcon ic;ic.FontFamily(FontFamily(L"Segoe Fluent Icons"));ic.Glyph(on?L"\uE735":L"\uE734");ic.FontSize(ScaledFont(owner.layout.settings.textSize,16));
 ic.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));
 favToggle.Content(ic);
 ToolTipService::SetToolTip(favToggle,box_value(i18n::Tr(L"收藏当前搜索词")));
}
void SearchWindow::OpenHit(SearchHit const& hit){
 NoteQuery();
 if(hit.kind==L"todo"){owner.ShowTodo();return;}
 if(hit.kind==L"note"){owner.ShowNote();return;}
 if(!hit.path.empty())OpenPath(hit.path);
}
void SearchWindow::Rebuild(std::wstring const& q){
 results.Children().Clear();hits.clear();
 if(q.empty()){
  auto favs=owner.layout.settings.searchFavorites;
  auto hist=owner.layout.settings.searchHistory;
  if(favs.empty()&&hist.empty()){
   hint.Visibility(Visibility::Visible);
   hint.Text(i18n::Tr(L"输入关键词，搜索分区内容、待办和便签。回车打开第一项。"));
   return;
  }
  hint.Visibility(Visibility::Collapsed);
  auto section=[&](wchar_t const* title){TextBlock t;t.Text(i18n::Tr(title));t.FontSize(ScaledFont(owner.layout.settings.textSize,11));t.Opacity(0.6);t.Margin(Thickness{10,8,10,2});results.Children().Append(t);};
  auto jump=[&](std::wstring const& text,wchar_t const* glyph){
   Grid g;g.Padding(Thickness{4,4,4,4});
   ColumnDefinition c0;c0.Width(GridLength{0,GridUnitType::Auto});g.ColumnDefinitions().Append(c0);
   ColumnDefinition c1;c1.Width(GridLength{1,GridUnitType::Star});g.ColumnDefinitions().Append(c1);
   FontIcon ic;ic.FontFamily(FontFamily(L"Segoe Fluent Icons"));ic.Glyph(glyph);ic.FontSize(ScaledFont(owner.layout.settings.textSize,14));ic.Margin(Thickness{2,1,12,0});ic.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));
   Grid::SetColumn(ic,0);g.Children().Append(ic);
   TextBlock name;name.Text(text);name.FontSize(ScaledFont(owner.layout.settings.textSize,13));name.TextTrimming(TextTrimming::CharacterEllipsis);
   auto jc=LabelChrome(name,ClampLabelStyle(owner.layout.settings.labelStyle),HorizontalAlignment::Left);Grid::SetColumn(jc,1);g.Children().Append(jc);
   Button row;row.HorizontalAlignment(HorizontalAlignment::Stretch);row.HorizontalContentAlignment(HorizontalAlignment::Stretch);row.Padding(Thickness{6,4,6,4});row.BorderThickness(Thickness{0});row.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));row.Content(g);
   row.Click([this,text](auto&&,auto&&){query.Text(text);});
   results.Children().Append(row);
  };
  if(!favs.empty()){section(L"收藏");for(auto const& f:favs)jump(f,L"\uE735");}
  if(!hist.empty()){
   section(L"最近搜索");
   for(auto const& h:hist)jump(h,L"\uE823");
   TextBlock cl;cl.Text(i18n::Tr(L"清除最近搜索"));cl.FontSize(ScaledFont(owner.layout.settings.textSize,11));cl.Opacity(0.6);cl.Margin(Thickness{10,6,10,2});
   Button clearBtn;clearBtn.HorizontalAlignment(HorizontalAlignment::Left);clearBtn.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));clearBtn.BorderThickness(Thickness{0});clearBtn.Padding(Thickness{4,2,4,2});clearBtn.MinWidth(0);clearBtn.Content(cl);
   clearBtn.Click([this](auto&&,auto&&){owner.layout.settings.searchHistory.clear();owner.Save();Rebuild(std::wstring(query.Text()));});
   results.Children().Append(clearBtn);
  }
  return;
 }
 SearchZones(owner.layout,q,hits);
 SearchWidgets(owner.layout,q,hits);
 if(hits.empty()){
  hint.Visibility(Visibility::Visible);
  hint.Text(i18n::Tr(L"分区和小组件里没有匹配的内容，可以试试网页搜索。"));
 }else hint.Visibility(Visibility::Collapsed);
 auto addRow=[&](Grid g){Button row;row.HorizontalAlignment(HorizontalAlignment::Stretch);row.HorizontalContentAlignment(HorizontalAlignment::Stretch);row.Padding(Thickness{6,4,6,4});row.BorderThickness(Thickness{0});row.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));row.Content(g);results.Children().Append(row);};
 for(auto const& hit:hits){
  Grid g;g.Padding(Thickness{4,4,4,4});
  ColumnDefinition c0;c0.Width(GridLength{0,GridUnitType::Auto});g.ColumnDefinitions().Append(c0);
  ColumnDefinition c1;c1.Width(GridLength{1,GridUnitType::Star});g.ColumnDefinitions().Append(c1);
  bool widget=hit.kind==L"todo"||hit.kind==L"note";
  if(widget){
   FontIcon glyph;glyph.FontFamily(FontFamily(L"Segoe Fluent Icons"));glyph.Glyph(hit.kind==L"todo"?L"\uE73A":L"\uE70B");glyph.FontSize(ScaledFont(owner.layout.settings.textSize,14));glyph.Margin(Thickness{2,1,12,0});glyph.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));
   Grid::SetColumn(glyph,0);g.Children().Append(glyph);
  }else{
   Image icon;icon.Width(20);icon.Height(20);icon.VerticalAlignment(VerticalAlignment::Top);icon.Margin(Thickness{0,1,10,0});
   Grid::SetColumn(icon,0);g.Children().Append(icon);
   try{shell::LoadIcon(hit.path,icon);}catch(...){}
  }
  StackPanel texts;texts.Spacing(1);
  TextBlock name;name.Text(hit.name);name.FontSize(ScaledFont(owner.layout.settings.textSize,13));name.TextTrimming(TextTrimming::CharacterEllipsis);
  TextBlock meta;meta.FontSize(ScaledFont(owner.layout.settings.textSize,11));meta.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));
  meta.Text(widget?i18n::Tr(hit.kind==L"todo"?L"待办":L"便签"):hit.zone+L" · "+hit.path);meta.TextTrimming(TextTrimming::CharacterEllipsis);
  texts.Children().Append(LabelChrome(name,ClampLabelStyle(owner.layout.settings.labelStyle),HorizontalAlignment::Left));texts.Children().Append(meta);
  Grid::SetColumn(texts,1);g.Children().Append(texts);
  addRow(g);
  auto row=results.Children().GetAt(results.Children().Size()-1).as<Button>();
  row.Click([this,hit](auto&&,auto&&){OpenHit(hit);});
 }
 if(!q.empty()){
  Grid g;g.Padding(Thickness{4,4,4,4});
  ColumnDefinition c0;c0.Width(GridLength{0,GridUnitType::Auto});g.ColumnDefinitions().Append(c0);
  ColumnDefinition c1;c1.Width(GridLength{1,GridUnitType::Star});g.ColumnDefinitions().Append(c1);
  FontIcon web;web.FontFamily(FontFamily(L"Segoe Fluent Icons"));web.Glyph(L"\uE721");web.FontSize(ScaledFont(owner.layout.settings.textSize,14));web.Margin(Thickness{0,2,10,0});web.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));
  Grid::SetColumn(web,0);g.Children().Append(web);
  TextBlock label;label.FontSize(ScaledFont(owner.layout.settings.textSize,13));label.Text(i18n::TrF(L"在浏览器中搜索「{0}」",{q}));
  Grid::SetColumn(label,1);g.Children().Append(label);
  addRow(g);
  auto row=results.Children().GetAt(results.Children().Size()-1).as<Button>();
  row.Click([this,q](auto&&,auto&&){NoteQuery();OpenPath(WebUrl(q));});
 }
 AppendEverything(q);
}
void SearchWindow::AppendEverything(std::wstring const& q){
 if(!ev::ShouldQuery(q,owner.layout.settings.everything))return;
 auto gen=++evGen;auto weak=std::weak_ptr<bool>(alive);auto dq=window.DispatcherQueue();
 std::thread([this,weak,dq,q,gen]{
  auto files=ev::Query(q,25);
  dq.TryEnqueue([this,weak,files,gen]{
   if(weak.lock()==nullptr||closing||gen!=evGen||files.empty())return;
   auto added=ev::MergeHits(hits,files);
   auto idx=results.Children().Size();if(idx>0)--idx;
   for(auto const& hit:added){
    Grid g;g.Padding(Thickness{4,4,4,4});
    ColumnDefinition c0;c0.Width(GridLength{0,GridUnitType::Auto});g.ColumnDefinitions().Append(c0);
    ColumnDefinition c1;c1.Width(GridLength{1,GridUnitType::Star});g.ColumnDefinitions().Append(c1);
    Image icon;icon.Width(20);icon.Height(20);icon.VerticalAlignment(VerticalAlignment::Top);icon.Margin(Thickness{0,1,10,0});
    Grid::SetColumn(icon,0);g.Children().Append(icon);
    try{shell::LoadIcon(hit.path,icon);}catch(...){}
    StackPanel texts;texts.Spacing(1);
    TextBlock name;name.Text(hit.name);name.FontSize(ScaledFont(owner.layout.settings.textSize,13));name.TextTrimming(TextTrimming::CharacterEllipsis);
    TextBlock meta;meta.FontSize(ScaledFont(owner.layout.settings.textSize,11));meta.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));meta.Text(L"Everything · "+hit.path);meta.TextTrimming(TextTrimming::CharacterEllipsis);
    texts.Children().Append(LabelChrome(name,ClampLabelStyle(owner.layout.settings.labelStyle),HorizontalAlignment::Left));texts.Children().Append(meta);
    Grid::SetColumn(texts,1);g.Children().Append(texts);
    Button row;row.HorizontalAlignment(HorizontalAlignment::Stretch);row.HorizontalContentAlignment(HorizontalAlignment::Stretch);row.Padding(Thickness{6,4,6,4});row.BorderThickness(Thickness{0});row.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));row.Content(g);
    row.Click([this,hit](auto&&,auto&&){OpenHit(hit);});
    results.Children().InsertAt(idx++,row);
   }
   hint.Visibility(Visibility::Collapsed);
  });
 }).detach();
}
void SearchWindow::SaveGeometry(){
 if(!IsWindow(hwnd))return;
 RECT r{};GetWindowRect(hwnd,&r);
 auto& w=owner.layout.widgets;
 w.searchX=r.left;w.searchY=r.top;
 MONITORINFOEXW mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
 w.searchMon=mi.szDevice;w.searchMX=r.left-mi.rcWork.left;w.searchMY=r.top-mi.rcWork.top;
 owner.Save();
}
SearchWindow::SearchWindow(Controller& c):owner(c){
 window=Window();window.Title(i18n::Tr(L"GuoDesk 搜索"));hwnd=shell::Handle(window);
 try{auto dir=std::filesystem::path(SearchExePath()).parent_path();window.AppWindow().SetIcon((dir/L"guodesk.ico").wstring());}catch(...){}
 window.SystemBackdrop(MakeBackdrop(owner.layout.settings.backdrop,owner.layout.settings.perfTier));
 try{window.AppWindow().IsShownInSwitchers(false);}catch(...){}
 root=Grid();
 GridLength rows[]={GridLength{0,GridUnitType::Auto},GridLength{1,GridUnitType::Star},GridLength{0,GridUnitType::Auto}};
 for(auto& r:rows){RowDefinition rd;rd.Height(r);root.RowDefinitions().Append(rd);}
 auto qBar=Grid();
 ColumnDefinition q0;q0.Width(GridLength{1,GridUnitType::Star});qBar.ColumnDefinitions().Append(q0);
 ColumnDefinition q1;q1.Width(GridLength{0,GridUnitType::Auto});qBar.ColumnDefinitions().Append(q1);
 query=TextBox();query.Margin(Thickness{12,12,4,8});query.PlaceholderText(i18n::Tr(L"搜索分区内容…"));query.FontSize(ScaledFont(owner.layout.settings.textSize,14));
 favToggle=Button();favToggle.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));favToggle.BorderThickness(Thickness{0});favToggle.Padding(Thickness{6,2,6,2});favToggle.MinWidth(0);favToggle.VerticalAlignment(VerticalAlignment::Center);favToggle.Margin(Thickness{4,12,12,8});
 favToggle.Click([this](auto&&,auto&&){auto q=std::wstring(query.Text());if(q.empty())return;ToggleSearchFavorite(owner.layout.settings.searchFavorites,q);owner.Save();UpdateFavGlyph(q);});
 Grid::SetColumn(query,0);Grid::SetColumn(favToggle,1);
 qBar.Children().Append(query);qBar.Children().Append(favToggle);
 Grid::SetRow(qBar,0);root.Children().Append(qBar);
 resultsHost=ScrollViewer();results=StackPanel();results.Spacing(2);results.Margin(Thickness{8,2,8,4});
 resultsHost.Content(results);resultsHost.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
 Grid::SetRow(resultsHost,1);root.Children().Append(resultsHost);
 hint=TextBlock();hint.Margin(Thickness{14,2,14,10});hint.FontSize(ScaledFont(owner.layout.settings.textSize,12));hint.TextWrapping(TextWrapping::Wrap);hint.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));
 Grid::SetRow(hint,2);root.Children().Append(hint);
 window.Content(root);
 query.TextChanged([this](auto&&,auto&&){auto q=std::wstring(query.Text());UpdateFavGlyph(q);Rebuild(q);});
 query.KeyDown([this](auto&&,Input::KeyRoutedEventArgs const& a){
  if(a.Key()==Windows::System::VirtualKey::Enter){
   a.Handled(true);
   if(!hits.empty())OpenHit(hits.front());
  }else if(a.Key()==Windows::System::VirtualKey::Escape){
   a.Handled(true);
   window.DispatcherQueue().TryEnqueue([this]{owner.CloseSearch();});
  }
 });
 root.KeyDown([this](auto&&,Input::KeyRoutedEventArgs const& a){
  if(a.Key()==Windows::System::VirtualKey::Escape)window.DispatcherQueue().TryEnqueue([this]{owner.CloseSearch();});
 });
 window.Closed([this](auto&&,auto&&){
  if(closing)return;closing=true;
  *alive=false;
  SaveGeometry();
  window.DispatcherQueue().TryEnqueue([this]{owner.CloseSearch();});
 });
 window.Activated([this](auto&&,auto&&args){
  if(args.WindowActivationState()==WindowActivationState::Deactivated)return;
  auto q=query;
  window.DispatcherQueue().TryEnqueue([q]{q.Focus(FocusState::Programmatic);});
 });
 query.Loaded([this](auto&&,auto&&){query.Focus(FocusState::Programmatic);});
 auto& w=owner.layout.widgets;
 int x=w.searchX,y=w.searchY,wd=560,ht=440;SetWindowPos(hwnd,nullptr,x,y,wd,ht,SWP_NOZORDER);
 MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
 if(y>mi.rcWork.bottom-80||y<mi.rcWork.top-20||x>mi.rcWork.right-80||x<mi.rcWork.left-40){x=std::clamp(x,(int)mi.rcWork.left,(int)mi.rcWork.right-100);y=std::clamp(y,(int)mi.rcWork.top,(int)mi.rcWork.bottom-80);SetWindowPos(hwnd,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}
 UpdateFavGlyph(L"");
 Rebuild(L"");
 window.Activate();
}
void SearchWindow::Show(){window.Activate();SetForegroundWindow(hwnd);}
SearchWindow::~SearchWindow(){closing=true;try{window.Closed(nullptr);}catch(...){}if(IsWindow(hwnd))window.Close();}
}
