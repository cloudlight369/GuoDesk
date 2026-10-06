#include "pch.h"
#include <shlwapi.h>
#include <psapi.h>
#include <thread>
#include <mutex>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Input.h>
#include "DeskWindow.h"
#include "TidyWindow.h"
#include "NoteWindow.h"
#include "TodoWindow.h"
#include "ClockWindow.h"
#include "GuideWindow.h"
#include "MusicWindow.h"
#include "SearchWindow.h"
#include "WeatherWindow.h"
#include "CaptureWindow.h"
#include "PreviewWindow.h"
#include "AppGridWindow.h"
#include "Shell.h"
#include "I18n.h"
#include "WebDav.h"
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Media::Animation;
using namespace Windows::ApplicationModel::DataTransfer;
namespace guodesk {
static Brush ResolveBrush(wchar_t const* key,Windows::UI::Color fallback,bool dark){
 try{
  auto res=Application::Current().Resources();
  return res.Lookup(box_value(key)).as<Brush>();
 }catch(...){}
 struct Entry{wchar_t const* key;Windows::UI::Color light;Windows::UI::Color dark;};
 static const Entry palette[]{
  {L"CardBackgroundFillColorDefault",{255,248,248,248},{255,43,43,43}},
  {L"CardBackgroundFillColorSecondary",{255,244,244,244},{255,54,54,54}},
  {L"SubtleFillColorSecondaryBrush",{255,236,236,236},{255,56,56,56}},
  {L"TextFillColorSecondary",{255,97,97,97},{255,199,199,199}},
  {L"ApplicationPageBackgroundThemeBrush",{255,243,243,243},{255,32,32,32}},
 };
 for(auto const& e:palette)if(!wcscmp(e.key,key))return SolidColorBrush(dark?e.dark:e.light);
 return SolidColorBrush(fallback);
}
Brush DeskWindow::ThemeBrush(wchar_t const* key,Windows::UI::Color fallback){return ResolveBrush(key,fallback,root.ActualTheme()==ElementTheme::Dark);}
static Button Action(wchar_t const* glyph,std::function<void()> run){FontIcon f;f.FontFamily(FontFamily(L"Segoe Fluent Icons"));f.Glyph(glyph);f.FontSize(16);Button b;b.Content(f);b.Background(nullptr);b.BorderThickness(Thickness{0});b.Click([run](auto&&,auto&&){try{run();}catch(hresult_error const& e){MessageBoxW(nullptr,e.message().c_str(),L"GuoDesk",MB_OK|MB_ICONERROR);}catch(std::exception const&){MessageBoxW(nullptr,i18n::Tr(L"操作未完成，请检查路径或访问权限。").c_str(),L"GuoDesk",MB_OK|MB_ICONERROR);}});return b;}
static MenuFlyoutItem MenuItem(std::wstring const& text,std::function<void()> run){MenuFlyoutItem b;b.Text(text);b.Click([run](auto&&,auto&&){try{run();}catch(...){MessageBoxW(nullptr,i18n::Tr(L"操作未完成，请检查文件是否存在及访问权限。").c_str(),L"GuoDesk",MB_OK|MB_ICONERROR);}});return b;}
template<class F>static void keepCapsuleOpen(DeskWindow* w,F const& f){auto alive=w->alive;f.Opened([alive,w](auto&&,auto&&){if(*alive)w->menuOpen=true;});f.Closed([alive,w](auto&&,auto&&){if(*alive)w->menuOpen=false;});}
static int Snap(int value,std::vector<int> const& targets,int threshold){int delta=0,best=threshold+1;for(int t:targets){int d=t-value,ad=d<0?-d:d;if(ad<best){best=ad;delta=d;}}return best<=threshold?delta:0;}
Zone& DeskWindow::Model(){return *std::find_if(owner.layout.zones.begin(),owner.layout.zones.end(),[this](auto const& z){return z.id==id;});}
struct DeskWindow::OpState{
 std::atomic<bool> cancel{false},finished{false};
 std::atomic<long long> done{0},total{0};
 std::wstring label;
 std::mutex gate;
 std::wstring current;
 std::function<void(shell::CancelFlag const&,shell::ProgressFn const&)> work;
 std::function<void()> after;
};
bool DeskWindow::Exists() const{return std::find_if(owner.layout.zones.begin(),owner.layout.zones.end(),[this](auto const& z){return z.id==id;})!=owner.layout.zones.end();}
Zone& DeskWindow::View(){auto& m=Model();auto members=GroupMemberIds(owner.layout,m.group);if(members.empty()||m.group.empty())return m;for(auto& z:owner.layout.zones)if(z.id==viewId)return z;viewId=m.id;return m;}
DeskWindow::DeskWindow(Controller& c,std::wstring key):owner(c),id(std::move(key)){
 window=Window();window.Title(L"GuoDesk");hwnd=shell::Handle(window);
 root=Grid();root.Padding(Thickness{12,12,12,12});root.RowSpacing(8);root.Tapped([this](auto&&,Input::TappedRoutedEventArgs const&){if(selected.empty()||GetAsyncKeyState(VK_CONTROL)&0x8000)return;selected.clear();Refresh();});
 root.PreviewKeyDown([this](auto&&,Input::KeyRoutedEventArgs const& a){OnNavKey(a);});
 RowDefinition tabsRow;tabsRow.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(tabsRow);RowDefinition header;header.Height(GridLength{44,GridUnitType::Pixel});root.RowDefinitions().Append(header);RowDefinition crumbs;crumbs.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(crumbs);RowDefinition body;body.Height(GridLength{1,GridUnitType::Star});root.RowDefinitions().Append(body);RowDefinition footer;footer.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(footer);RowDefinition pinsRow;pinsRow.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(pinsRow);
 tabsPanel=StackPanel();tabsPanel.Orientation(Orientation::Horizontal);tabsPanel.Spacing(4);tabsPanel.VerticalAlignment(VerticalAlignment::Center);Grid::SetRow(tabsPanel,0);tabsPanel.Visibility(Visibility::Collapsed);root.Children().Append(tabsPanel);
 crumbBar=StackPanel();crumbBar.Orientation(Orientation::Horizontal);crumbBar.Spacing(2);crumbBar.VerticalAlignment(VerticalAlignment::Center);Grid::SetRow(crumbBar,2);crumbBar.Visibility(Visibility::Collapsed);root.Children().Append(crumbBar);
 Grid bar;ColumnDefinition col;col.Width(GridLength{1,GridUnitType::Star});bar.ColumnDefinitions().Append(col);ColumnDefinition buttons;buttons.Width(GridLength{0,GridUnitType::Auto});bar.ColumnDefinitions().Append(buttons);Grid::SetRow(bar,1);barGrid=bar;
 title=TextBox();title.Text(Model().name);title.PlaceholderText(i18n::Tr(L"分区名称"));title.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));title.BorderThickness(Thickness{0});title.FontSize(ScaledFont(owner.layout.settings.textSize,16));title.FontWeight(Windows::UI::Text::FontWeights::SemiBold());title.VerticalContentAlignment(VerticalAlignment::Center);title.Padding(Thickness{4,11,4,11});ToolTipService::SetToolTip(title,box_value(i18n::Tr(L"点击重命名分区")));auto TitleFill=[this](bool hover){Brush fill=SolidColorBrush(Windows::UI::Colors::Transparent());if(hover)fill=ThemeBrush(L"SubtleFillColorSecondaryBrush",Windows::UI::Color{13,0,0,0});title.Background(fill);};title.PointerEntered([TitleFill](auto&&,auto&&){TitleFill(true);});title.PointerExited([TitleFill](auto&&,auto&&){TitleFill(false);});title.LostFocus([this](auto&&,auto&&){auto name=std::wstring(title.Text());if(name.empty())name=i18n::Tr(L"未命名分区");View().name=name;title.Text(name);window.Title(L"GuoDesk · "+name);owner.Save();});bar.Children().Append(title);
 StackPanel actions;actions.Orientation(Orientation::Horizontal);actions.Spacing(4);actions.VerticalAlignment(VerticalAlignment::Center);Grid::SetColumn(actions,1);
 Border dragHandle;FontIcon dragIcon;dragIcon.FontFamily(FontFamily(L"Segoe Fluent Icons"));dragIcon.Glyph(L"\uE7C2");dragIcon.FontSize(16);dragHandle.Child(dragIcon);dragHandle.Padding(Thickness{6,4,6,4});dragHandle.CornerRadius(CornerRadius{4,4,4,4});dragHandle.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));ToolTipService::SetToolTip(dragHandle,box_value(i18n::Tr(L"按住移动分区")));auto HandleHover=[dragHandle,this](bool hover){Brush fill=SolidColorBrush(Windows::UI::Colors::Transparent());if(hover)fill=ThemeBrush(L"SubtleFillColorSecondaryBrush",Windows::UI::Color{13,0,0,0});dragHandle.Background(fill);};dragHandle.PointerEntered([dragHandle,HandleHover](auto&&,auto&&){HandleHover(true);});dragHandle.PointerExited([dragHandle,HandleHover](auto&&,auto&&){HandleHover(false);});embedTimer=root.DispatcherQueue().CreateTimer();embedTimer.Interval(std::chrono::milliseconds(16));embedTimer.Tick([this](auto&&,auto&&){if(!dragging){embedTimer.Stop();return;}if(!(GetAsyncKeyState(VK_LBUTTON)&0x8000)){embedTimer.Stop();dragging=false;Capture();owner.Save();return;}DragUpdate();});dragHandle.PointerPressed([this,dragHandle](auto&&,Input::PointerRoutedEventArgs const& a)mutable{if(Model().locked){Notify(i18n::Tr(L"分区已锁定：点左上角锁形图标或右键菜单解锁后才能移动。"));a.Handled(true);return;}dragging=true;GetCursorPos(&dragStart);GetWindowRect(hwnd,&dragOrigin);if(!desktop)dragHandle.CapturePointer(a.Pointer());else embedTimer.Start();a.Handled(true);});auto DragMove=[this](auto&&,auto&&){DragUpdate();};dragHandle.PointerMoved(DragMove);root.PointerMoved(DragMove);title.PointerMoved(DragMove);auto EndDrag=[this](auto&&,auto&&){if(!dragging)return;dragging=false;Capture();owner.Save();};dragHandle.PointerReleased(EndDrag);dragHandle.PointerCaptureLost(EndDrag);root.PointerReleased(EndDrag);actions.Children().Append(dragHandle);
auto add=Action(L"\uE710",[this]{Pick();});ToolTipService::SetToolTip(add,box_value(i18n::Tr(L"添加入口")));add.PointerMoved(DragMove);actions.Children().Append(add);
 chevron=Action(L"\uE70E",[this]{SetCollapsed(!Model().collapsed);});ToolTipService::SetToolTip(chevron,box_value(i18n::Tr(L"折叠 / 展开")));chevron.PointerMoved(DragMove);actions.Children().Append(chevron);
 lockBtn=Action(L"\uE72E",[this]{ToggleLock();});ToolTipService::SetToolTip(lockBtn,box_value(i18n::Tr(L"分区已锁定 · 点击解锁")));lockBtn.PointerMoved(DragMove);lockBtn.Visibility(Visibility::Collapsed);actions.Children().InsertAt(1,lockBtn);
 auto more=Action(L"\uE712",[]{});ToolTipService::SetToolTip(more,box_value(i18n::Tr(L"更多操作")));more.Click([this,more](auto&&,auto&&){Menu(more);});more.PointerMoved(DragMove);actions.Children().Append(more);bar.Children().Append(actions);root.Children().Append(bar);
 grid=GridView();grid.SelectionMode(ListViewSelectionMode::None);grid.IsItemClickEnabled(false);grid.AllowDrop(true);grid.HorizontalAlignment(HorizontalAlignment::Stretch);Grid::SetRow(grid,3);root.Children().Append(grid);
 grid.DragOver([this](auto&&,DragEventArgs const& a){OnZoneDragOver(a);});grid.Drop([this](auto&&,DragEventArgs const& a){OnZoneDrop(a);});
 listHost=ScrollViewer();listPanel=StackPanel();listPanel.Orientation(Orientation::Vertical);listPanel.Spacing(2);listPanel.Padding(Thickness{2,2,2,2});listHost.Content(listPanel);listHost.AllowDrop(true);listHost.Visibility(Visibility::Collapsed);Grid::SetRow(listHost,3);root.Children().Append(listHost);
 listHost.DragOver([this](auto&&,DragEventArgs const& a){OnZoneDragOver(a);});listHost.Drop([this](auto&&,DragEventArgs const& a){OnZoneDrop(a);});
 grid.PointerPressed([this](auto&&,Input::PointerRoutedEventArgs const&){FocusBody();});listHost.PointerPressed([this](auto&&,Input::PointerRoutedEventArgs const&){FocusBody();});
 status=TextBlock();status.FontSize(11);status.TextWrapping(TextWrapping::Wrap);status.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,160,160,160}));
 opBar=ProgressBar();opBar.Minimum(0);opBar.Maximum(100);opBar.Height(4);opBar.ShowError(false);opBar.CornerRadius(CornerRadius{2,2,2,2});opBar.Visibility(Visibility::Collapsed);
 opCancelBtn=Button();opCancelBtn.Content(box_value(i18n::Tr(L"取消")));opCancelBtn.FontSize(11);opCancelBtn.Padding(Thickness{8,2,8,2});opCancelBtn.Margin(Thickness{8,0,0,0});opCancelBtn.VerticalAlignment(VerticalAlignment::Center);opCancelBtn.Visibility(Visibility::Collapsed);opCancelBtn.Click([this](auto&&,auto&&){CancelOp();});
 Grid opRow;ColumnDefinition opText;opText.Width(GridLength{1,GridUnitType::Star});opRow.ColumnDefinitions().Append(opText);ColumnDefinition opBtn;opBtn.Width(GridLength{0,GridUnitType::Auto});opRow.ColumnDefinitions().Append(opBtn);Grid::SetColumn(status,0);opRow.Children().Append(status);Grid::SetColumn(opCancelBtn,1);opRow.Children().Append(opCancelBtn);
 StackPanel footBox;footBox.Orientation(Orientation::Vertical);footBox.Spacing(4);footBox.Children().Append(opBar);footBox.Children().Append(opRow);Grid::SetRow(footBox,4);root.Children().Append(footBox);
 opTimer=root.DispatcherQueue().CreateTimer();opTimer.Interval(std::chrono::milliseconds(120));opTimer.Tick([this](auto&&,auto&&){OpTick();});
 pinBar=StackPanel();pinBar.Orientation(Orientation::Horizontal);pinBar.Spacing(6);pinBar.Margin(Thickness{10,0,10,10});pinBar.HorizontalAlignment(HorizontalAlignment::Left);Grid::SetRow(pinBar,5);root.Children().Append(pinBar);
 auto members=GroupMemberIds(owner.layout,Model().group);viewId=members.empty()?id:(Model().groupTab<static_cast<int>(members.size())?members[static_cast<size_t>(Model().groupTab)]:members.front());
 pill=Border();pill.CornerRadius(CornerRadius{20,20,20,20});pill.Height(36);pill.Padding(Thickness{10,0,10,0});pill.VerticalAlignment(VerticalAlignment::Center);pill.HorizontalAlignment(HorizontalAlignment::Stretch);Grid::SetRow(pill,1);StackPanel pillBox;pillBox.Orientation(Orientation::Horizontal);pillBox.Spacing(8);pillIcon=Image();pillIcon.Width(20);pillIcon.Height(20);pillGlyph=FontIcon();pillGlyph.FontFamily(FontFamily(L"Segoe Fluent Icons"));pillGlyph.Glyph(L"\uE8B7");pillGlyph.FontSize(16);pillName=TextBlock();pillName.FontSize(ScaledFont(owner.layout.settings.textSize,13));pillName.FontWeight(Windows::UI::Text::FontWeights::SemiBold());pillName.VerticalAlignment(VerticalAlignment::Center);pillName.TextTrimming(TextTrimming::CharacterEllipsis);pillBox.Children().Append(pillGlyph);pillBox.Children().Append(pillIcon);pillBox.Children().Append(pillName);pill.Child(pillBox);FontIcon grip;grip.FontFamily(FontFamily(L"Segoe Fluent Icons"));grip.Glyph(L"\uE7C2");grip.FontSize(14);grip.VerticalAlignment(VerticalAlignment::Center);pillBox.Children().InsertAt(0,grip);pill.Visibility(Visibility::Collapsed);pill.PointerPressed([this](auto&&,Input::PointerRoutedEventArgs const& a)mutable{if(Model().locked){Notify(i18n::Tr(L"分区已锁定：点左上角锁形图标或右键菜单解锁后才能移动。"));a.Handled(true);return;}dragging=true;GetCursorPos(&dragStart);GetWindowRect(hwnd,&dragOrigin);if(!desktop)pill.CapturePointer(a.Pointer());else embedTimer.Start();a.Handled(true);});pill.PointerMoved(DragMove);pill.PointerReleased(EndDrag);pill.PointerCaptureLost(EndDrag);pill.DoubleTapped([this](auto&&,auto&&){SetCapsule(false);Refresh();Place();owner.Save();});pill.ContextFlyout(nullptr);pill.RightTapped([this](auto&&,auto&&){Menu(pill);});root.Children().Append(pill);
 hoverTimer=root.DispatcherQueue().CreateTimer();hoverTimer.Interval(std::chrono::milliseconds(80));hoverTimer.Tick([this](auto&&,auto&&){if(dragging||closing)return;if(menuOpen){ExpandCapsule();return;}POINT p{};GetCursorPos(&p);RECT r{};GetWindowRect(hwnd,&r);if(PtInRect(&r,p))ExpandCapsule();else ShrinkCapsule();});
 if(Model().capsule)SetCapsule(true);
 window.Content(root);window.Closed([this](auto&&,WindowEventArgs const& args){if(!closing){args.Handled(true);ShowWindow(hwnd,SW_HIDE);}});
 SetWindowSubclass(hwnd,Subclass,1,reinterpret_cast<DWORD_PTR>(this));window.Activate();auto presenter=window.AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();presenter.SetBorderAndTitleBar(true,false);window.AppWindow().IsShownInSwitchers(false);ApplySettings();Place();
 if(!owner.layout.settings.performance){Storyboard fade;DoubleAnimation alpha;alpha.From(0.0);alpha.To(1.0);alpha.Duration(Duration{std::chrono::milliseconds(220),DurationType::TimeSpan});Storyboard::SetTarget(alpha,root);Storyboard::SetTargetProperty(alpha,L"Opacity");fade.Children().Append(alpha);fade.Begin();}
}
DeskWindow::~DeskWindow(){*alive=false;closing=true;if(hoverTimer)hoverTimer.Stop();if(embedTimer)embedTimer.Stop();if(hoverTabTimer)hoverTabTimer.Stop();if(opTimer)opTimer.Stop();
 // 工作线程只持有共享状态；退出前请求取消并做有界等待，避免进程在 SHFileOperation 中途销毁
 if(opState){opState->cancel.store(true);for(int i=0;i<60&&!opState->finished.load();++i)Sleep(50);opState.reset();}
 opRunning=false;
 try{window.Closed(nullptr);}catch(...){}if(IsWindow(hwnd)){RemoveWindowSubclass(hwnd,Subclass,1);window.Close();}}
winrt::Microsoft::UI::Xaml::Media::SystemBackdrop MakeBackdrop(int kind){
 if(kind==1)try{return winrt::Microsoft::UI::Xaml::Media::SystemBackdrop{winrt::Microsoft::UI::Xaml::Media::DesktopAcrylicBackdrop()};}catch(...){}
 try{return winrt::Microsoft::UI::Xaml::Media::SystemBackdrop{winrt::Microsoft::UI::Xaml::Media::MicaBackdrop()};}catch(...){return winrt::Microsoft::UI::Xaml::Media::SystemBackdrop{nullptr};}
}
void DeskWindow::ApplyPerformance(){
 bool perf=owner.layout.settings.performance;
 try{window.SystemBackdrop(perf?winrt::Microsoft::UI::Xaml::Media::SystemBackdrop{nullptr}:MakeBackdrop(owner.layout.settings.backdrop));}catch(...){}
 ApplyBackground();
}
void DeskWindow::ApplyBackground(){
 auto& v=View();bool perf=owner.layout.settings.performance;
 if(v.background==bgPath&&v.dim==bgDim&&perf==bgPerf&&desktop==bgDesk)return;
 bgPath=v.background;bgDim=v.dim;bgPerf=perf;bgDesk=desktop;
 Brush next{nullptr};bool isImg=false;
 if(!bgPath.empty()&&IsImagePath(bgPath)&&GetFileAttributesW(bgPath.c_str())!=INVALID_FILE_ATTRIBUTES){
  try{wchar_t url[1024];DWORD c=1024;if(SUCCEEDED(UrlCreateFromPathW(bgPath.c_str(),url,&c,0))){winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage img;img.UriSource(winrt::Windows::Foundation::Uri(url));ImageBrush ib;ib.ImageSource(img);ib.Stretch(Stretch::Fill);next=ib;isImg=true;}}catch(...){}
 }
 if(!isImg)next=(perf||desktop)?ThemeBrush(L"ApplicationPageBackgroundThemeBrush",Windows::UI::Color{255,32,32,32}):Brush{nullptr};
 root.Background(next);
 if(!dimLayer){Border d;d.IsHitTestVisible(false);Grid::SetRowSpan(d,6);dimLayer=d;root.Children().InsertAt(0,d);}
 uint8_t a=isImg?(bgDim==0?0:bgDim==1?72:146):0;
 dimLayer.Background(SolidColorBrush(Windows::UI::Color{a,0,0,0}));
}
void DeskWindow::ApplySettings(){auto const& theme=owner.layout.settings.theme;root.RequestedTheme(theme==L"Dark"?ElementTheme::Dark:theme==L"Light"?ElementTheme::Light:ElementTheme::Default);ApplyPerformance();if(!owner.layout.settings.tabHover){hoverTabPending=-1;if(hoverTabTimer)hoverTabTimer.Stop();}status.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,160,160,160}));Refresh();}
void DeskWindow::Notify(std::wstring const& text){status.Text(text);}
void DeskWindow::Capture(){if(!IsWindow(hwnd))return;RECT r{};GetWindowRect(hwnd,&r);auto& z=Model();z.x=r.left;z.y=r.top;if(!capsuleNow)z.width=r.right-r.left;MONITORINFOEXW mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);if(!z.collapsed&&!capsuleNow){int live=r.bottom-r.top;if(live<ZoneCapHeight(z.maxHeight,mi.rcWork.bottom-mi.rcWork.top))z.height=live;}z.mon=mi.szDevice;z.mx=z.x-mi.rcWork.left;z.my=z.y-mi.rcWork.top;}
RECT DeskWindow::WorkRect(){MONITORINFOEXW mi{sizeof(mi)};if(!GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi))mi.rcWork=RECT{0,0,1920,1080};return mi.rcWork;}
void DeskWindow::SetCollapsed(bool on){auto& z=Model();if(z.collapsed==on)return;Capture();RECT wk=WorkRect();int full=ZoneExpandedHeight(z.height,z.maxHeight,wk.bottom-wk.top);int from=z.collapsed?88:full,to=on?88:full;Reanchor(from,to);z.collapsed=on;Refresh();Place();owner.Save();}
void DeskWindow::Reanchor(int fromHeight,int toHeight){if(fromHeight==toHeight)return;RECT wk=WorkRect();auto& z=Model();z.y=ZoneAnchorTop(z.y,fromHeight,toHeight,z.expandDir,wk.top,wk.bottom);}
void DeskWindow::ToggleLock(){auto& z=Model();z.locked=!z.locked;Refresh();owner.Save();Notify(z.locked?i18n::Tr(L"分区已锁定：位置与尺寸已固定，且不可删除分区。"):i18n::Tr(L"分区已解锁，可继续拖动与调整大小。"));}
void DeskWindow::Place(){auto& z=Model();shell::Fit(z);POINT p{z.x,z.y};if(desktop&&GetParent(hwnd))ScreenToClient(GetParent(hwnd),&p);RECT wk=WorkRect();int w=z.width,h=z.collapsed?88:ZoneExpandedHeight(z.height,z.maxHeight,wk.bottom-wk.top);if(capsuleNow){w=MulDiv(160,GetDpiForWindow(hwnd),96);h=MulDiv(68,GetDpiForWindow(hwnd),96);}SetWindowPos(hwnd,nullptr,p.x,p.y,w,h,SWP_NOZORDER|SWP_NOACTIVATE);}
void DeskWindow::RebuildTabs(){auto& z=Model();auto members=GroupMemberIds(owner.layout,z.group);bool show=!z.group.empty()&&members.size()>1&&!capsuleNow&&!z.collapsed;tabsPanel.Visibility(show?Visibility::Visible:Visibility::Collapsed);if(!show)return;tabsPanel.Children().Clear();
 for(size_t idx=0;idx<members.size();++idx){
  auto zit=std::find_if(owner.layout.zones.begin(),owner.layout.zones.end(),[&](auto const& o){return o.id==members[idx];});
  if(zit==owner.layout.zones.end())continue;
  bool active=zit->id==viewId;
  auto tab=Button();tab.Background(nullptr);tab.BorderThickness(Thickness{0});tab.Padding(Thickness{10,4,10,4});tab.CornerRadius(CornerRadius{6,6,6,6});
  TextBlock t;t.Text(zit->name);t.FontSize(ScaledFont(owner.layout.settings.textSize,12));t.TextTrimming(TextTrimming::CharacterEllipsis);t.MaxWidth(96);
  if(active){t.FontWeight(Windows::UI::Text::FontWeights::SemiBold());tab.Background(ThemeBrush(L"CardBackgroundFillColorDefault",Windows::UI::Color{255,60,60,60}));}
  tab.Content(t);
  auto captured=static_cast<int>(idx);
  tab.Click([this,captured](auto&&,auto&&){SwitchTab(captured);});
  tab.PointerEntered([this,captured](auto&&,auto&&){if(owner.layout.settings.tabHover)HoverTabSwitch(captured);});
  tab.PointerExited([this](auto&&,auto&&){hoverTabPending=-1;if(hoverTabTimer)hoverTabTimer.Stop();});
  tabsPanel.Children().Append(tab);
 }
 auto plus=Button();plus.Background(nullptr);plus.BorderThickness(Thickness{0});plus.Padding(Thickness{6,4,6,4});FontIcon pg;pg.FontFamily(FontFamily(L"Segoe Fluent Icons"));pg.Glyph(L"\uE710");pg.FontSize(12);plus.Content(pg);ToolTipService::SetToolTip(plus,box_value(i18n::Tr(L"在标签组中新增分区")));plus.Click([this](auto&&,auto&&){owner.AddToGroup(id);});tabsPanel.Children().Append(plus);}
void DeskWindow::SwitchTab(int index){auto& z=Model();auto members=GroupMemberIds(owner.layout,z.group);if(members.empty())return;index=std::clamp(index,0,static_cast<int>(members.size())-1);viewId=members[static_cast<size_t>(index)];z.groupTab=index;expandedStack.clear();selected.clear();focusIdx=-1;Refresh();owner.Save();}
void DeskWindow::HoverTabSwitch(int index){
 hoverTabPending=index;
 if(!hoverTabTimer){
  hoverTabTimer=root.DispatcherQueue().CreateTimer();
  hoverTabTimer.Interval(std::chrono::milliseconds(320));
  hoverTabTimer.Tick([this](auto&&,auto&&){
   auto target=hoverTabPending;hoverTabPending=-1;hoverTabTimer.Stop();
   if(target<0||dragging||closing||!IsWindow(hwnd))return;
   POINT p{};GetCursorPos(&p);RECT r{};GetWindowRect(hwnd,&r);
   if(!PtInRect(&r,p))return;
   auto members=GroupMemberIds(owner.layout,Model().group);
   if(members.empty()||target>=static_cast<int>(members.size()))return;
   if(members[static_cast<size_t>(target)]==viewId)return;
   SwitchTab(target);
  });
 }
 hoverTabTimer.Stop();hoverTabTimer.Start();
}
void DeskWindow::Navigate(std::wstring const& folder){auto& v=View();v.browseFolder=(v.browseInPlace&&!v.mappedFolder.empty()&&UnderRoot(v.mappedFolder,folder))?folder:std::wstring();expandedStack.clear();selected.clear();focusIdx=-1;navPaths.clear();Refresh();owner.Save();}
void DeskWindow::RenderCrumbs(){if(!crumbBar)return;auto& z=Model();auto& v=View();bool browsing=!v.mappedFolder.empty()&&v.browseInPlace&&UnderRoot(v.mappedFolder,v.browseFolder);bool show=browsing&&!z.collapsed&&!capsuleNow;crumbBar.Visibility(show?Visibility::Visible:Visibility::Collapsed);if(!show)return;crumbBar.Children().Clear();auto chain=Crumbs(v.mappedFolder,v.browseFolder);for(size_t i=0;i<chain.size();++i){if(i>0){TextBlock sep;sep.Text(L"\u203A");sep.FontSize(11);sep.VerticalAlignment(VerticalAlignment::Center);sep.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,160,160,160}));crumbBar.Children().Append(sep);}std::wstring label=i==0?shell::Name(v.mappedFolder):std::filesystem::path(chain[i]).filename().wstring();if(i+1<chain.size()){auto target=chain[i];Button b;b.Background(nullptr);b.BorderThickness(Thickness{0});b.Padding(Thickness{4,1,4,1});b.CornerRadius(CornerRadius{4,4,4,4});TextBlock t;t.Text(label);t.FontSize(ScaledFont(owner.layout.settings.textSize,12));t.TextTrimming(TextTrimming::CharacterEllipsis);t.MaxWidth(120);b.Content(t);b.Click([this,target](auto&&,auto&&){Navigate(target);});crumbBar.Children().Append(b);}else{TextBlock cur;cur.Text(label);cur.FontSize(12);cur.FontWeight(Windows::UI::Text::FontWeights::SemiBold());cur.TextTrimming(TextTrimming::CharacterEllipsis);cur.MaxWidth(160);cur.VerticalAlignment(VerticalAlignment::Center);crumbBar.Children().Append(cur);}}crumbBar.Visibility(Visibility::Visible);}
void DeskWindow::SetCapsule(bool on){auto& z=Model();z.capsule=on;capsuleNow=on;if(on){z.collapsed=false;if(hoverTimer)hoverTimer.Start();}else{if(hoverTimer)hoverTimer.Stop();pill.Visibility(Visibility::Collapsed);}Refresh();Place();}
void DeskWindow::ExpandCapsule(){if(!capsuleNow||dragging)return;capsuleNow=false;Refresh();Place();}
void DeskWindow::ShrinkCapsule(){if(capsuleNow||dragging)return;Capture();capsuleNow=true;Refresh();Place();}
void DeskWindow::AttachDrag(FrameworkElement const& el,std::wstring const& path){
 el.PointerPressed([this,path](auto&&,Input::PointerRoutedEventArgs const&){FocusBody();int idx=-1;for(size_t i=0;i<navPaths.size();++i)if(PathKey(navPaths[i])==PathKey(path)){idx=static_cast<int>(i);break;}if(idx>=0)SetFocus(idx);POINT sp{};GetCursorPos(&sp);dragSX=sp.x;dragSY=sp.y;dragPath=path;dragArmed=true;});
 el.PointerReleased([this](auto&&,Input::PointerRoutedEventArgs const&){dragArmed=false;});
 el.PointerCaptureLost([this](auto&&,auto&&){dragArmed=false;});
 if(!rootDragHooked){rootDragHooked=true;
  root.PointerMoved([this](auto&&,Input::PointerRoutedEventArgs const&){if(!dragArmed)return;POINT cp{};GetCursorPos(&cp);long dx=cp.x-dragSX,dy=cp.y-dragSY;if(dx*dx+dy*dy>36){dragArmed=false;std::wstring h=dragPath;root.DispatcherQueue().TryEnqueue([this,h]{std::vector<std::wstring> out;auto k=PathKey(h);if(std::find(selected.begin(),selected.end(),k)!=selected.end()&&selected.size()>1){for(auto const& s:selected)for(auto const& e:View().entries)if(PathKey(e.path)==s&&GetFileAttributesW(e.path.c_str())!=INVALID_FILE_ATTRIBUTES){out.push_back(e.path);break;}}else if(GetFileAttributesW(h.c_str())!=INVALID_FILE_ATTRIBUTES)out.push_back(h);if(!out.empty())shell::DragOut(hwnd,out);});}});
 }
}
bool DeskWindow::IsSel(std::wstring const& path){return std::find(selected.begin(),selected.end(),PathKey(path))!=selected.end();}
void DeskWindow::ToggleSel(std::wstring const& path){ToggleSelect(selected,PathKey(path));Refresh();}
int DeskWindow::NavIndex(std::wstring const& path){for(size_t i=0;i<navPaths.size();++i)if(PathKey(navPaths[i])==PathKey(path))return static_cast<int>(i);return -1;}
void DeskWindow::TapSelect(std::wstring const& path){
 FocusBody();
 bool shift=(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0,ctrl=(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0;
 int index=NavIndex(path);
 if(shift&&selAnchor>=0&&index>=0){SelectRange(selected,navPaths,selAnchor,index);SetFocus(index);SelHint();return;}
 if(ctrl){selAnchor=index;ToggleSel(path);SetFocus(index);SelHint();FocusBody();return;}
 selAnchor=index;selected.clear();selected.push_back(PathKey(path));SetFocus(index);SelHint();
}
void DeskWindow::BeginOp(std::wstring const& label,std::function<void(shell::CancelFlag const&,shell::ProgressFn const&)> work,std::function<void()> finish){
 if(opRunning){Notify(i18n::Tr(L"上一个文件操作还在进行，请稍候或点击“取消”。"));return;}
 auto st=std::make_shared<OpState>();
 st->label=label;st->work=std::move(work);st->after=std::move(finish);
 opState=st;opRunning=true;
 opBar.Value(0);opBar.Visibility(Visibility::Visible);opCancelBtn.Visibility(Visibility::Visible);opCancelBtn.IsEnabled(true);
 status.Text(label);
 auto body=[st]{
  auto co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  try{
   shell::CancelFlag cancel(st,&st->cancel);
   shell::ProgressFn prog=[st](long long done,long long total,std::wstring const& cur){st->done.store(done);st->total.store(total);{std::lock_guard<std::mutex>lk(st->gate);st->current=cur;}};
   st->work(cancel,prog);
  }catch(...){}
  st->work=nullptr;
  st->finished.store(true);
  if(co==S_OK)CoUninitialize();
 };
 // 线程创建失败（资源耗尽）时必须复位互斥标志，否则此后所有文件操作都会被"上一个还在进行"挡死
 try{std::thread(body).detach();}
 catch(...){
  opRunning=false;opState.reset();
  opBar.Visibility(Visibility::Collapsed);opCancelBtn.Visibility(Visibility::Collapsed);
  Notify(i18n::Tr(L"无法启动后台任务：系统资源不足，请稍候重试。"));
  return;
 }
 opTimer.Start();
}
void DeskWindow::OpTick(){
 if(!opRunning||!opState){opTimer.Stop();return;}
 auto st=opState;
 if(st->finished.load()){
  opRunning=false;opTimer.Stop();
  opBar.Visibility(Visibility::Collapsed);opCancelBtn.Visibility(Visibility::Collapsed);
  opState.reset();
  auto after=std::move(st->after);
  if(after)after();
  return;
 }
 long long const done=st->done.load(),total=st->total.load();
 std::wstring cur;{std::lock_guard<std::mutex>lk(st->gate);cur=st->current;}
 opBar.Value(static_cast<double>(ProgressPercent(done,total)));
 std::wstring text=st->label;text+=L" ";text+=std::to_wstring(done);text+=L"/";text+=std::to_wstring(total);
 if(!cur.empty()){text+=L" · ";text+=cur;}
 status.Text(text);
}
void DeskWindow::CancelOp(){
 if(!opState)return;
 opState->cancel.store(true);
 opCancelBtn.IsEnabled(false);
 Notify(i18n::Tr(L"正在取消：已完成的条目不会回滚。"));
}
std::vector<std::wstring> DeskWindow::ListedPaths(){
 auto& v=View();std::vector<std::wstring> out;
 if(!v.mappedFolder.empty()&&!v.browseFolder.empty()){for(auto const& e:ListMapped(v.browseFolder))out.push_back(e.path);return out;}
 for(auto const& e:v.entries)out.push_back(e.path);
 return out;
}
std::vector<std::wstring> DeskWindow::OpPaths(){
 std::vector<std::wstring> out;
 if(!selected.empty()){for(auto const& p:ListedPaths())if(IsSel(p))out.push_back(p);return out;}
 if(focusIdx>=0&&focusIdx<static_cast<int>(navPaths.size()))out.push_back(navPaths[focusIdx]);
 return out;
}
std::wstring DeskWindow::TargetFolder(){auto& v=View();if(v.mappedFolder.empty())return std::wstring();return v.browseFolder.empty()?v.mappedFolder:v.browseFolder;}
void DeskWindow::CopyClip(bool cut){
 auto paths=OpPaths();
 if(paths.empty()){Notify(i18n::Tr(L"请先选中要复制的条目。"));return;}
 try{shell::ClipboardCopy(paths,cut);}catch(...){Notify(i18n::Tr(L"复制未完成：剪贴板正被其它程序占用。"));return;}
 Notify(i18n::TrF(cut?L"已剪切 {0} 项，可在目标分区或资源管理器按 Ctrl+V。":L"已复制 {0} 项，可按 Ctrl+V 粘贴。",{std::to_wstring(paths.size())}));
}
void DeskWindow::PasteClip(){
 if(opRunning){Notify(i18n::Tr(L"上一个文件操作还在进行，请稍候或点击“取消”。"));return;}
 auto clip=shell::ReadClipFiles();
 if(clip.paths.empty()){Notify(i18n::Tr(clip.busy?L"剪贴板正被其它程序占用，请稍候重试。":L"剪贴板中没有文件。"));return;}
 auto here=TargetFolder();
 if(here.empty()){
  auto& v=View();
  owner.PushUndo(i18n::TrF(L"粘贴 {0} 个入口到分区「{1}」",{std::to_wstring(clip.paths.size()),v.name}));
  int added=0;
  for(auto const& p:clip.paths){if(std::any_of(v.entries.begin(),v.entries.end(),[&](auto const& x){return PathKey(x.path)==PathKey(p);}))continue;if(AddEntry(v,p))++added;}
  Refresh();owner.Save();
  Notify(i18n::TrF(L"已按引用添加 {0} 个入口，原文件未移动。",{std::to_wstring(added)}));
  return;
 }
 long long const total=static_cast<long long>(clip.paths.size());
 auto res=std::make_shared<shell::TransferResult>();
 BeginOp(i18n::TrF(L"正在粘贴 {0} 项…",{std::to_wstring(total)}),
  [clip,here,res](shell::CancelFlag const& cancel,shell::ProgressFn const& prog){*res=shell::TransferFiles(clip.paths,here,clip.move,cancel,prog);},
  [this,guard=alive,res,here,total]{
   if(!*guard)return;
   auto& q=View();
   long long const ok=static_cast<long long>(res->made.size());
   SyncMapped(q);
   selected.clear();for(auto const& p:res->made)if(selected.size()<50)selected.push_back(PathKey(p));
   focusIdx=-1;Refresh();owner.Save();
   auto state=OpOutcome(ok,res->failed,res->cancelled);
   std::wstring msg;
   if(state==3)msg=i18n::Tr(L"粘贴失败：文件可能被占用或目标文件夹不可写。");
   else if(state==2)msg=i18n::TrF(L"已粘贴 {0} 项，{1} 项失败。",{std::to_wstring(ok),std::to_wstring(res->failed)});
   else if(!ok)msg=i18n::Tr(res->cancelled?L"已取消粘贴，未复制任何文件。":L"粘贴未完成：文件已在目标文件夹中。");
   else if(res->skipped)msg=i18n::TrF(L"已粘贴 {0} 项到「{1}」，跳过 {2} 项（已在目标文件夹中）。",{std::to_wstring(ok),shell::Name(here),std::to_wstring(res->skipped)});
   else msg=i18n::TrF(L"已粘贴 {0} 项到「{1}」。",{std::to_wstring(ok),shell::Name(here)});
   if(res->cancelled&&ok)msg+=L" "+i18n::TrF(L"已取消剩余 {0} 项。",{std::to_wstring(std::max<long long>(0,total-ok-res->skipped-res->failed))});
   Notify(msg);
  });
}
void DeskWindow::CreateFolderHere(){
 if(opRunning){Notify(i18n::Tr(L"上一个文件操作还在进行，请稍候或点击“取消”。"));return;}
 auto here=TargetFolder();
 if(here.empty()){Notify(i18n::Tr(L"普通分区请用「添加文件夹」，映射分区可直接在此新建文件夹。"));return;}
 std::wstring made;
 try{made=shell::CreateFolder(here,i18n::Tr(L"新建文件夹"));}catch(...){Notify(i18n::Tr(L"新建文件夹未完成：目标文件夹不可写或已被删除。"));return;}
 SyncMapped(View());Refresh();owner.Save();
 Notify(i18n::TrF(L"已新建文件夹「{0}」。",{shell::Name(made)}));
}
void DeskWindow::ArchiveHere(){
 if(opRunning){Notify(i18n::Tr(L"上一个文件操作还在进行，请稍候或点击“取消”。"));return;}
 auto here=TargetFolder();
 if(here.empty()){Notify(i18n::Tr(L"普通分区没有映射文件夹，无法按规则归档。"));return;}
 auto const files=ListLooseFiles(here,false,1000);
 std::vector<std::wstring> unmatched;
 auto groups=ArchivePlan(owner.layout.rules,files,&unmatched);
 long long hits=0;for(auto const& g:groups)hits+=static_cast<long long>(g.paths.size());
 if(groups.empty()){Notify(files.empty()?i18n::TrF(L"「{0}」里没有可归档的散文件。",{shell::Name(here)}):i18n::TrF(L"{0} 个文件都没匹配上规则，请先在设置里配置规则。",{std::to_wstring(files.size())}));return;}
 ContentDialog dlg;
 dlg.Title(box_value(i18n::Tr(L"按规则归档")));
 StackPanel box;box.Orientation(Orientation::Vertical);box.Spacing(5);box.MaxWidth(340);
 TextBlock head;head.Text(i18n::TrF(L"将把「{0}」里 {1} 个文件移入分类子文件夹：",{shell::Name(here),std::to_wstring(hits)}));head.TextWrapping(TextWrapping::Wrap);box.Children().Append(head);
 for(auto const& g:groups){TextBlock line;line.Text(L"· "+g.category+L" · "+std::to_wstring(g.paths.size()));line.FontSize(12);box.Children().Append(line);}
 if(!unmatched.empty()){TextBlock rest;rest.Text(i18n::TrF(L"另有 {0} 项不匹配规则，保持原位。",{std::to_wstring(unmatched.size())}));rest.FontSize(12);rest.Opacity(0.75);box.Children().Append(rest);}
 // 列举有上限，逼近上限时必须说清楚，否则用户以为一次就归档干净了
 if(files.size()>=1000){TextBlock cap;cap.Text(i18n::TrF(L"这里只列出前 {0} 个散文件，其余保持原位，可再归档一次。",{std::to_wstring(files.size())}));cap.FontSize(11);cap.Opacity(0.7);cap.TextWrapping(TextWrapping::Wrap);box.Children().Append(cap);}
 TextBlock note;note.Text(i18n::Tr(L"文件会被移动到本文件夹下的子文件夹，不会删除任何内容。"));note.FontSize(11);note.Opacity(0.7);note.TextWrapping(TextWrapping::Wrap);box.Children().Append(note);
 dlg.Content(box);
 dlg.PrimaryButtonText(i18n::TrF(L"移动 {0} 个文件",{std::to_wstring(hits)}));dlg.CloseButtonText(i18n::Tr(L"取消"));dlg.DefaultButton(ContentDialogButton::Close);
 try{dlg.XamlRoot(root.XamlRoot());}catch(...){return;}
 auto guard=this->alive;
 if(opDialog){Notify(i18n::Tr(L"上一个文件操作还在进行，请稍候或点击“取消”。"));return;}opDialog=true;
 auto plan=std::make_shared<std::vector<ArchiveGroup>>(std::move(groups));
 // ShowAsync 在 XamlRoot 失效时直接抛异常，那时 Completed 永远不跑，opDialog 会被永久占住
 try{
  dlg.ShowAsync().Completed([this,guard,plan,here](auto&& async,auto&&){
   if(!*guard)return;
   opDialog=false;
   if(async.GetResults()!=ContentDialogResult::Primary)return;
   RunArchive(plan,here);
  });
 }catch(...){opDialog=false;Notify(i18n::Tr(L"归档对话框没能打开，请重试。"));}
}
void DeskWindow::RunArchive(std::shared_ptr<std::vector<ArchiveGroup>> plan,std::wstring here){
 long long total=0;for(auto const& g:*plan)total+=static_cast<long long>(g.paths.size());
 auto res=std::make_shared<shell::TransferResult>();
 auto cats=std::make_shared<long long>(0);
 auto blocked=std::make_shared<long long>(0);
 BeginOp(i18n::TrF(L"正在归档 {0} 项…",{std::to_wstring(total)}),
  [plan,here,res,cats,blocked](shell::CancelFlag const& cancel,shell::ProgressFn const& prog){
   long long base=0,totalAll=0;for(auto const& g:*plan)totalAll+=static_cast<long long>(g.paths.size());
   for(auto const& g:*plan){
    if(cancel->load())break;
    auto const count=static_cast<long long>(g.paths.size());
    std::error_code ec;auto dir=std::filesystem::path(here)/g.category;
    // 根目录里躺着一个正好和分类同名的文件：整组跳过，既不动它，也不会把文件写进"文件"里
    std::error_code ke;
    if(std::filesystem::exists(dir,ke)&&!std::filesystem::is_directory(dir,ke)){*blocked+=count;base+=count;prog(base,totalAll,g.category);continue;}
    std::filesystem::create_directories(dir,ec);
    // 建不出子文件夹就是整组做不了，记账后继续下一组，别让一个分类卡住整批归档
    if(ec){res->failed+=count;base+=count;prog(base,totalAll,g.category);continue;}
    // 分类里已经有同名文件就跳过：TransferFiles 遇到重名会改名成副本，而归档要的是归位，不是多出一份
    std::vector<std::wstring> items;
    for(auto const& s:g.paths){std::error_code ne;if(std::filesystem::exists(dir/std::filesystem::path(s).filename(),ne)){++res->skipped;continue;}items.push_back(s);}
    if(items.empty()){base+=count;prog(base,totalAll,std::wstring());continue;}
    auto part=shell::TransferFiles(items,dir.wstring(),true,cancel,[&](long long d,long long,std::wstring const& name){prog(base+d,totalAll,name);});
    res->made.insert(res->made.end(),part.made.begin(),part.made.end());
    res->skipped+=part.skipped;res->failed+=part.failed;res->cancelled=res->cancelled||part.cancelled;
    if(!part.made.empty())++(*cats);
    base+=count;prog(base,totalAll,std::wstring());
   }
  },
  [this,guard=alive,res,cats,blocked,total,plan]{
   if(!*guard)return;
   auto& v=View();
   SyncMapped(v);
   selected.clear();for(auto const& p:res->made)if(selected.size()<50)selected.push_back(PathKey(p));
   focusIdx=-1;Refresh();owner.Save();
   long long const ok=static_cast<long long>(res->made.size());
   auto const state=OpOutcome(ok,res->failed,res->cancelled);
   std::wstring text;
   if(state==3)text=i18n::Tr(L"归档未完成：文件可能被占用或文件夹不可写。");
   else if(state==4)text=i18n::Tr(L"已取消归档，未移动任何文件。");
   else if(!ok)text=i18n::Tr(L"没有文件被移动（分类文件夹里已有同名文件）。");
   else text=i18n::TrF(L"已把 {0} 个文件归档到 {1} 个分类文件夹。",{std::to_wstring(ok),std::to_wstring(*cats)});
   if(ok&&res->failed)text+=L" "+i18n::TrF(L"{0} 项归档失败。",{std::to_wstring(res->failed)});
   if(ok&&res->skipped)text+=L" "+i18n::TrF(L"跳过 {0} 项（分类文件夹里已有同名文件）。",{std::to_wstring(res->skipped)});
   if(*blocked)text+=L" "+i18n::TrF(L"{0} 项没动：根目录里有一个和分类同名的文件。",{std::to_wstring(*blocked)});
   if(res->cancelled&&ok)text+=L" "+i18n::TrF(L"已取消剩余 {0} 项。",{std::to_wstring(std::max<long long>(0,total-ok-res->skipped-res->failed-*blocked))});
   Notify(text);
  });
}
void DeskWindow::PreviewSelection(){
 auto list=navPaths.empty()?ListedPaths():navPaths;
 if(list.empty()){Notify(i18n::Tr(L"没有可预览的条目。"));return;}
 size_t start=0;
 if(focusIdx>=0&&focusIdx<static_cast<int>(list.size()))start=static_cast<size_t>(focusIdx);
 else if(!selected.empty())for(size_t i=0;i<list.size();++i)if(PathKey(list[i])==selected.front()){start=i;break;}
 owner.ShowPreview(list,start);
}
void DeskWindow::PreviewPath(std::wstring const& path){
 auto list=navPaths.empty()?ListedPaths():navPaths;
 auto hit=std::find_if(list.begin(),list.end(),[&](auto const& p){return PathKey(p)==PathKey(path);});
 if(hit==list.end()){owner.ShowPreview({path},0);return;}
 owner.ShowPreview(list,static_cast<size_t>(std::distance(list.begin(),hit)));
}
void DeskWindow::RenameOne(){
 auto paths=OpPaths();
 if(paths.size()!=1){Notify(i18n::Tr(L"请只选中一个条目再重命名。"));return;}
 if(opRunning){Notify(i18n::Tr(L"上一个文件操作还在进行，请稍候或点击“取消”。"));return;}
 auto path=paths.front();
 TextBox box;box.Text(std::filesystem::path(path).filename().wstring());
 ContentDialog dlg;dlg.Title(box_value(i18n::Tr(L"重命名")));dlg.PrimaryButtonText(i18n::Tr(L"保存"));dlg.CloseButtonText(i18n::Tr(L"取消"));dlg.DefaultButton(ContentDialogButton::Primary);
 StackPanel p;p.Spacing(8);p.MaxWidth(320);p.Children().Append(box);dlg.Content(p);
 try{dlg.XamlRoot(root.XamlRoot());}catch(...){return;}
 auto guard=this->alive;
 if(opDialog)return;opDialog=true;
 dlg.ShowAsync().Completed([this,guard,path,box](auto&& async,auto&&){
  if(!*guard)return;
  opDialog=false;
  if(async.GetResults()!=ContentDialogResult::Primary)return;
  auto t=std::wstring(box.Text());
  size_t a=t.find_first_not_of(L" \t");
  if(a==std::wstring::npos)return;
  t=t.substr(a,t.find_last_not_of(L" \t")-a+1);
  auto oldKey=PathKey(path),parent=std::filesystem::path(path).parent_path().wstring();
  try{shell::RenamePath(path,t);}catch(...){Notify(i18n::Tr(L"重命名未完成：名称重复、是系统保留名，或含有 \\ / : * ? \" < > |。"));return;}
  auto& v=View();for(auto& e:v.entries)if(PathKey(e.path)==oldKey)e.path=parent+L"\\"+t;
  if(!v.mappedFolder.empty())SyncMapped(v);
  selected.clear();focusIdx=-1;Refresh();owner.Save();
  Notify(i18n::TrF(L"已重命名为「{0}」。",{t}));
 });
}
void DeskWindow::DeleteSelected(bool permanent){
 auto paths=OpPaths();
 if(paths.empty()){Notify(i18n::Tr(L"请先选中要删除的条目。"));return;}
 if(opRunning){Notify(i18n::Tr(L"上一个文件操作还在进行，请稍候或点击“取消”。"));return;}
 long long unrecycled=permanent?0:shell::CountNonRecyclable(paths);
 ContentDialog dlg;
 dlg.Title(box_value(i18n::Tr(permanent?L"彻底删除文件":L"移到回收站")));
 TextBlock msg;msg.Text(i18n::TrF(permanent?L"将直接从磁盘删除 {0} 项，不经过回收站，可能无法找回：{1}":L"将把 {0} 项移到回收站，可从回收站还原：{1}",{std::to_wstring(paths.size()),shell::Name(paths.front())}));
 if(unrecycled)msg.Text(msg.Text()+L"\n"+i18n::TrF(L"其中 {0} 项所在的卷不支持回收站，会被彻底删除。",{std::to_wstring(unrecycled)}));
 msg.TextWrapping(TextWrapping::Wrap);msg.MaxWidth(320);
 dlg.Content(msg);
 dlg.PrimaryButtonText(i18n::Tr(permanent?L"仍然删除":L"移到回收站"));dlg.CloseButtonText(i18n::Tr(L"取消"));dlg.DefaultButton(ContentDialogButton::Close);
 try{dlg.XamlRoot(root.XamlRoot());}catch(...){return;}
 auto guard=this->alive;
 if(opDialog)return;opDialog=true;
 dlg.ShowAsync().Completed([this,guard,paths,permanent](auto&& async,auto&&){
  if(!*guard)return;
  opDialog=false;
  if(async.GetResults()!=ContentDialogResult::Primary)return;
  auto res=std::make_shared<shell::DeleteResult>();
  long long const total=static_cast<long long>(paths.size());
  BeginOp(i18n::TrF(permanent?L"正在彻底删除 {0} 项…":L"正在移到回收站 {0} 项…",{std::to_wstring(total)}),
   [paths,permanent,res](shell::CancelFlag const& cancel,shell::ProgressFn const& prog){*res=shell::DeleteFiles(paths,permanent,cancel,prog);},
   [this,guard,res,permanent,total]{
   if(!*guard)return;
   auto& v=View();
   std::vector<std::wstring> keys;for(auto const& p:res->gone)keys.push_back(PathKey(p));
   auto hit=[&](auto const& x){return std::find(keys.begin(),keys.end(),PathKey(x.path))!=keys.end();};
   if(!v.mappedFolder.empty())SyncMapped(v);
   else{
    size_t n=std::count_if(v.entries.begin(),v.entries.end(),hit);
    if(n){owner.PushUndo(i18n::TrF(L"移除 {0} 个已删除条目的入口",{std::to_wstring(n)}));std::erase_if(v.entries,hit);}
   }
   selected.clear();focusIdx=-1;Refresh();owner.Save();
   long long const ok=static_cast<long long>(res->gone.size());
   auto const state=OpOutcome(ok,res->failed,res->cancelled);
   std::wstring text;
   if(state==3)text=i18n::Tr(L"删除未完成：文件可能被占用或需要权限。");
   else if(state==4)text=i18n::Tr(L"已取消删除，未删除任何文件。");
   else text=i18n::TrF(permanent?L"已彻底删除 {0} 项。":L"已把 {0} 项移到回收站。",{std::to_wstring(ok)});
   if(ok&&res->failed)text+=L" "+i18n::TrF(L"{0} 项删除失败。",{std::to_wstring(res->failed)});
   if(ok&&res->cancelled)text+=L" "+i18n::TrF(L"已取消剩余 {0} 项。",{std::to_wstring(std::max<long long>(0,total-ok-res->failed))});
   if(ok&&res->nuked)text+=L" "+i18n::TrF(L"其中 {0} 项所在卷没有回收站，已彻底删除。",{std::to_wstring(res->nuked)});
   Notify(text);
  });
 });
}
winrt::Microsoft::UI::Xaml::Media::Brush DeskWindow::ItemFill(std::wstring const& path){
 if(!IsSel(path))return ThemeBrush(L"CardBackgroundFillColorDefault",Windows::UI::Color{255,60,60,60});
 Windows::UI::Color c{80,0,120,215};
 try{if(auto b=ThemeBrush(L"SystemAccentColor",Windows::UI::Color{255,0,120,215}).as<SolidColorBrush>()){auto a=b.Color();c=Windows::UI::Color{80,a.R,a.G,a.B};}}catch(...){}
 return SolidColorBrush(c);
}
winrt::Microsoft::UI::Xaml::Media::Brush DeskWindow::FocusRing(){Windows::UI::Color c{255,0,120,215};try{if(auto b=ThemeBrush(L"SystemAccentColor",Windows::UI::Color{255,0,120,215}).as<SolidColorBrush>()){auto a=b.Color();c=Windows::UI::Color{255,a.R,a.G,a.B};}}catch(...){}return SolidColorBrush(c);}
void DeskWindow::FocusBody(){auto& v=View();try{if(v.viewMode==L"list")listHost.Focus(FocusState::Pointer);else grid.Focus(FocusState::Pointer);}catch(...){}}
void DeskWindow::RegisterNav(std::wstring const& path,winrt::Microsoft::UI::Xaml::Controls::Border const& el,bool tinted){if(Model().collapsed)return;navPaths.push_back(path);navVis.push_back(NavVis{el,tinted});}
void DeskWindow::RepaintNav(){
 for(size_t i=0;i<navPaths.size()&&i<navVis.size();++i){auto const& vis=navVis[i];if(!vis.el)continue;
  if(vis.tinted)vis.el.Background(ItemFill(navPaths[i]));
  bool on=static_cast<int>(i)==focusIdx;
  vis.el.BorderThickness(on?Thickness{2,2,2,2}:Thickness{0,0,0,0});
  vis.el.BorderBrush(on?Brush(FocusRing()):Brush{nullptr});
 }
}
void DeskWindow::SetFocus(int index){
 focusIdx=index;RepaintNav();
 try{if(index>=0&&index<static_cast<int>(navVis.size())&&navVis[index].el)navVis[index].el.StartBringIntoView();}catch(...){}
}
void DeskWindow::MoveFocus(int delta){
 int const n=static_cast<int>(navPaths.size());if(n==0){focusIdx=-1;return;}
 SetFocus(NavStep(focusIdx,n,delta));
}
void DeskWindow::SelHint(){Notify(selected.empty()?i18n::Tr(L"双击打开 · 右键管理 · 拖拽排序 · 单击后方向键选择"):i18n::TrF(L"已选 {0} 项 · 拖出包含全部选中",{std::to_wstring(selected.size())}));}
void DeskWindow::TryOpen(std::wstring const& path){try{shell::Open(hwnd,path);}catch(...){Notify(i18n::Tr(L"无法打开目标，请右键重新定位。"));}}
void DeskWindow::TryReveal(std::wstring const& path){try{shell::Reveal(hwnd,path);}catch(...){Notify(i18n::Tr(L"无法定位原文件，目标可能已被移动或删除。"));}}
void DeskWindow::OpenFocused(std::wstring const& path){auto& q=View();auto fa=GetFileAttributesW(path.c_str());if(!q.mappedFolder.empty()&&q.browseInPlace&&fa!=INVALID_FILE_ATTRIBUTES&&(fa&FILE_ATTRIBUTE_DIRECTORY)){Navigate(path);return;}TryOpen(path);}
void DeskWindow::OnNavKey(Input::KeyRoutedEventArgs const& a){
 if(capsuleNow||menuOpen||Model().collapsed||title.FocusState()!=FocusState::Unfocused)return;
 auto key=a.Key();bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
 if(key==Windows::System::VirtualKey::Escape){if(!selected.empty()||focusIdx>=0||!expandedStack.empty()){selected.clear();focusIdx=-1;expandedStack.clear();Refresh();}a.Handled(true);return;}
 if(ctrl&&key==Windows::System::VirtualKey::Left){auto& z=Model();auto members=GroupMemberIds(owner.layout,z.group);if(!members.empty()){SwitchTab(std::max(0,z.groupTab-1));a.Handled(true);return;}}
 if(ctrl&&key==Windows::System::VirtualKey::Right){auto& z=Model();auto members=GroupMemberIds(owner.layout,z.group);if(!members.empty()){SwitchTab(std::min(static_cast<int>(members.size())-1,z.groupTab+1));a.Handled(true);return;}}
 winrt::Windows::Foundation::IInspectable focused{nullptr};
 try{focused=Input::FocusManager::GetFocusedElement(root.XamlRoot());}catch(...){}
 if(focused&&(focused.try_as<Primitives::ButtonBase>()||focused.try_as<TextBox>()))return;
 if(navPaths.empty())return;
 if(key==Windows::System::VirtualKey::Up||key==Windows::System::VirtualKey::Left){MoveFocus(-1);a.Handled(true);return;}
 if(key==Windows::System::VirtualKey::Down||key==Windows::System::VirtualKey::Right){MoveFocus(1);a.Handled(true);return;}
 if(key==Windows::System::VirtualKey::Home){SetFocus(0);a.Handled(true);return;}
 if(key==Windows::System::VirtualKey::End){SetFocus(static_cast<int>(navPaths.size())-1);a.Handled(true);return;}
 if(key==Windows::System::VirtualKey::F2){RenameOne();a.Handled(true);return;}
 if(key==Windows::System::VirtualKey::Delete){DeleteSelected((GetKeyState(VK_SHIFT)&0x8000)!=0);a.Handled(true);return;}
 if(ctrl&&key==Windows::System::VirtualKey::C){CopyClip(false);a.Handled(true);return;}
 if(ctrl&&key==Windows::System::VirtualKey::X){CopyClip(true);a.Handled(true);return;}
 if(ctrl&&key==Windows::System::VirtualKey::V){PasteClip();a.Handled(true);return;}
 if(ctrl&&key==Windows::System::VirtualKey::A){
  auto keys=selected;
  for(auto const& p:ListedPaths()){if(keys.size()>=50)break;auto k=PathKey(p);if(std::find(keys.begin(),keys.end(),k)==keys.end())keys.push_back(k);}
  selected=std::move(keys);RepaintNav();SelHint();a.Handled(true);return;
 }
 if(key==Windows::System::VirtualKey::Space&&!ctrl){PreviewSelection();a.Handled(true);return;}
 if(focusIdx<0||focusIdx>=static_cast<int>(navPaths.size()))return;
 auto path=navPaths[focusIdx];
 if(key==Windows::System::VirtualKey::Enter){OpenFocused(path);a.Handled(true);return;}
 if(key==Windows::System::VirtualKey::Space){ToggleSelect(selected,PathKey(path));RepaintNav();SelHint();a.Handled(true);return;}
}
std::vector<RECT> DeskWindow::Peers(){std::vector<RECT> list;for(auto& w:owner.windows)if(w.get()!=this&&IsWindow(w->hwnd)&&IsWindowVisible(w->hwnd)){RECT r{};GetWindowRect(w->hwnd,&r);list.push_back(r);}return list;}
void DeskWindow::DragUpdate(){if(!dragging||Model().locked)return;POINT p{};GetCursorPos(&p);int x=dragOrigin.left+p.x-dragStart.x,y=dragOrigin.top+p.y-dragStart.y,w=dragOrigin.right-dragOrigin.left,h=dragOrigin.bottom-dragOrigin.top;int threshold=MulDiv(8,GetDpiForWindow(hwnd),96);std::vector<int> xs,ys;auto peers=Peers();for(auto const& r:peers){int cx=r.left+(r.right-r.left)/2,cy=r.top+(r.bottom-r.top)/2;xs.push_back(r.left);xs.push_back(r.right-w);xs.push_back(r.left-w);xs.push_back(r.right);xs.push_back(cx-w/2);ys.push_back(r.top);ys.push_back(r.bottom-h);ys.push_back(r.top-h);ys.push_back(r.bottom);ys.push_back(cy-h/2);}MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);xs.push_back(mi.rcWork.left);xs.push_back(mi.rcWork.right-w);ys.push_back(mi.rcWork.top);ys.push_back(mi.rcWork.bottom-h);x+=Snap(x,xs,threshold);y+=Snap(y,ys,threshold);SetWindowPos(hwnd,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}
void DeskWindow::SnapResize(RECT& r,int edge){auto peers=Peers();if(peers.empty())return;int threshold=MulDiv(10,GetDpiForWindow(hwnd),96);bool left=edge==WMSZ_LEFT||edge==WMSZ_TOPLEFT||edge==WMSZ_BOTTOMLEFT,top=edge==WMSZ_TOP||edge==WMSZ_TOPLEFT||edge==WMSZ_TOPRIGHT;int w=r.right-r.left,best=threshold+1,target=w;for(auto const& p:peers){int pw=p.right-p.left,d=pw-w,ad=d<0?-d:d;if(ad<best){best=ad;target=pw;}}if(best<=threshold){if(left)r.left=r.right-target;else r.right=r.left+target;}int h=r.bottom-r.top;best=threshold+1;target=h;for(auto const& p:peers){int ph=p.bottom-p.top,d=ph-h,ad=d<0?-d:d;if(ad<best){best=ad;target=ph;}}if(best<=threshold){if(top)r.top=r.bottom-target;else r.bottom=r.top+target;}}
void DeskWindow::CapResize(RECT& r,int edge){auto& z=Model();if(z.collapsed||capsuleNow||z.maxHeight<=0)return;RECT wk=WorkRect();int cap=ZoneCapHeight(z.maxHeight,wk.bottom-wk.top);if(r.bottom-r.top<=cap)return;if(edge==WMSZ_TOP||edge==WMSZ_TOPLEFT||edge==WMSZ_TOPRIGHT)r.top=r.bottom-cap;else r.bottom=r.top+cap;}
void DeskWindow::Show(){ShowWindow(hwnd,SW_SHOWNOACTIVATE);}
void DeskWindow::Raise(bool on){
 if(!IsWindow(hwnd)||raised==on)return;
 RECT r{};GetWindowRect(hwnd,&r);
 int w=r.right-r.left,h=r.bottom-r.top;
 if(on){raised=true;if(desktop)shell::Detach(hwnd);SetWindowPos(hwnd,HWND_TOPMOST,r.left,r.top,w,h,SWP_FRAMECHANGED|SWP_NOACTIVATE);return;}
 raised=false;
 if(desktop&&shell::Attach(hwnd,owner.host))return;
 SetWindowPos(hwnd,HWND_BOTTOM,r.left,r.top,w,h,SWP_FRAMECHANGED|SWP_NOACTIVATE);
 if(!desktop)return;
}
void DeskWindow::EmbedRetry(){if(!IsWindow(hwnd)||!desktop||GetParent(hwnd))return;if(shell::Attach(hwnd,owner.host))Place();}
void DeskWindow::SetDesktop(bool enabled){Capture();bool was=desktop;if(enabled){desktop=shell::Attach(hwnd,owner.host);ApplyPerformance();Notify(desktop?i18n::Tr(L"实验性桌面宿主 · 添加入口不会移动原文件"):i18n::Tr(L"嵌入失败，已保留普通窗口模式"));}else{if(desktop)shell::Detach(hwnd);desktop=false;ApplyPerformance();Notify(i18n::Tr(L"普通窗口模式 · 可在菜单中试验桌面嵌入"));}Place();if(desktop!=was){RECT r{};GetWindowRect(hwnd,&r);int width=r.right-r.left,height=r.bottom-r.top;SetWindowPos(hwnd,nullptr,0,0,width,height+1,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);SetWindowPos(hwnd,nullptr,0,0,width,height,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);}}
void DeskWindow::Pick(bool folder){if(!View().mappedFolder.empty()){Notify(i18n::Tr(L"映射分区为只读视图：请在资源管理器中修改文件夹后右键刷新。"));return;}for(auto const& path:shell::Pick(hwnd,folder))AddEntry(View(),path);Refresh();owner.Save();}
void DeskWindow::RebuildPins(){auto& m=Model();auto& v=View();pinBar.Children().Clear();bool show=!v.pins.empty()&&!m.collapsed;pinBar.Visibility(show?Visibility::Visible:Visibility::Collapsed);if(!show)return;
 for(auto const& p:v.pins){auto const path=p;Button b;b.Width(40);b.Height(40);b.Padding(Thickness{7,7,7,7});b.CornerRadius(CornerRadius{8,8,8,8});b.Background(ThemeBrush(L"CardBackgroundFillColorSecondary",Windows::UI::Color{255,90,90,90}));Image img;img.Width(26);img.Height(26);b.Content(img);shell::LoadIcon(path,img);ToolTipService::SetToolTip(b,box_value(shell::Name(path)));if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES)b.Opacity(0.35);
  b.Click([this,path](auto&&,auto&&){if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES){Notify(i18n::Tr(L"快捷方式目标已不存在。"));return;}TryOpen(path);});
  MenuFlyout fly;fly.Items().Append(MenuItem(i18n::Tr(L"从快捷栏取消钉选"),[this,path]{auto& q=View();std::erase_if(q.pins,[&](auto const& s){return PathKey(s)==PathKey(path);});Refresh();owner.Save();}));keepCapsuleOpen(this,fly);b.ContextFlyout(fly);
  pinBar.Children().Append(b);}
}
void DeskWindow::Menu(FrameworkElement const& target){auto& z=Model();auto& v=View();bool mapped=!v.mappedFolder.empty();MenuFlyout menu;
 if(!owner.undo.Empty()){auto* ctrl=&owner;menu.Items().Append(MenuItem(i18n::TrF(L"撤销：{0}",{owner.undo.TopLabel()}),[ctrl]{ctrl->Undo();}));menu.Items().Append(MenuFlyoutSeparator());}
 menu.Items().Append(MenuItem(i18n::Tr(L"添加文件 / 应用"),[this,mapped]{if(mapped){Notify(i18n::Tr(L"映射分区为只读视图：请在资源管理器中修改文件夹后右键刷新。"));return;}Pick();}));
 menu.Items().Append(MenuItem(i18n::Tr(L"添加文件夹"),[this,mapped]{if(mapped){Notify(i18n::Tr(L"映射分区为只读视图：请在资源管理器中修改文件夹后右键刷新。"));return;}Pick(true);}));
 menu.Items().Append(MenuItem(i18n::Tr(L"新建文件夹…"),[this,mapped]{if(!mapped){Notify(i18n::Tr(L"普通分区请用「添加文件夹」，映射分区可直接在此新建文件夹。"));return;}CreateFolderHere();}));
 menu.Items().Append(MenuItem(i18n::Tr(L"按规则归档此文件夹…"),[this,mapped]{if(!mapped){Notify(i18n::Tr(L"普通分区没有映射文件夹，无法按规则归档。"));return;}ArchiveHere();}));
 menu.Items().Append(MenuItem(shell::HasClipFiles()?(mapped?i18n::Tr(L"粘贴文件到此处"):i18n::Tr(L"粘贴为入口")):i18n::Tr(L"粘贴（剪贴板无文件）"),[this]{PasteClip();}));
 menu.Items().Append(MenuItem(i18n::Tr(L"重命名选中项…"),[this]{RenameOne();}));
 menu.Items().Append(MenuItem(i18n::Tr(L"删除选中项（回收站）…"),[this]{DeleteSelected(false);}));
 menu.Items().Append(MenuItem(i18n::Tr(L"钉选快捷方式…"),[this]{auto picked=shell::Pick(hwnd);if(picked.empty())return;auto& m=View();int added=0;for(auto const& p:picked){if(m.pins.size()>=12)break;if(std::any_of(m.pins.begin(),m.pins.end(),[&](auto const& q){return PathKey(q)==PathKey(p);}))continue;owner.PushUndo(i18n::TrF(L"钉选快捷方式「{0}」",{shell::Name(p)}));m.pins.push_back(p);++added;}if(added){Refresh();owner.Save();Notify(i18n::TrF(L"已钉选 {0} 个快捷方式。",{std::to_wstring(added)}));}}));
 menu.Items().Append(MenuItem(mapped?i18n::Tr(L"取消文件夹映射"):i18n::Tr(L"映射文件夹…"),[this,mapped]{if(mapped){View().mappedFolder.clear();View().browseFolder.clear();Refresh();owner.Save();Notify(i18n::Tr(L"已取消映射，恢复普通分区。"));return;}auto picked=shell::Pick(hwnd,true);if(picked.empty())return;View().mappedFolder=picked.front();View().browseFolder.clear();SyncMapped(View());Refresh();owner.Save();Notify(i18n::Tr(L"已映射文件夹（只读视图）：修改请在资源管理器中完成，右键可刷新。"));}));
 if(mapped){ToggleMenuFlyoutItem browse;browse.Text(i18n::Tr(L"文件夹就地打开"));browse.IsChecked(v.browseInPlace);browse.Click([this](auto&&,auto&&){auto& m=View();m.browseInPlace=!m.browseInPlace;if(!m.browseInPlace)m.browseFolder.clear();Refresh();owner.Save();});menu.Items().Append(browse);}
 menu.Items().Append(MenuItem(v.viewMode==L"list"?i18n::Tr(L"切换为图标视图"):i18n::Tr(L"切换为列表视图"),[this]{auto& m=View();m.viewMode=m.viewMode==L"list"?L"grid":L"list";Refresh();owner.Save();}));
 if(!mapped){MenuFlyoutSubItem sort;
  auto addKey=[&](wchar_t const* label,wchar_t const* value){sort.Items().Append(MenuItem(i18n::Tr(label),[this,value,label]{auto& m=View();m.sortKey=value;m.sortDescending=false;SortEntries(m,SortKeyFromString(value),false);Refresh();owner.Save();Notify(i18n::TrF(L"已按 {0} 排序",{std::wstring(i18n::Tr(label))}));}));};
  addKey(L"按名称",L"name");addKey(L"按类型",L"type");addKey(L"按修改日期",L"date");addKey(L"按大小",L"size");
  ToggleMenuFlyoutItem desc;desc.Text(i18n::Tr(L"降序"));desc.IsChecked(v.sortDescending);desc.Click([this](auto&&,auto&&){auto& m=View();m.sortDescending=!m.sortDescending;if(!m.sortKey.empty())SortEntries(m,SortKeyFromString(m.sortKey),m.sortDescending);Refresh();owner.Save();});
  sort.Items().Append(desc);sort.Text(i18n::Tr(L"排序"));menu.Items().Append(sort);}
 menu.Items().Append(MenuItem(i18n::TrF(L"图标大小：{0}（点击切换）",{std::wstring(i18n::Tr(v.tileSize==0?L"小":v.tileSize==1?L"中":L"大"))}),[this]{auto& m=View();m.tileSize=(m.tileSize+1)%3;Refresh();owner.Save();}));
 menu.Items().Append(MenuItem(i18n::TrF(L"文件名：{0}（点击切换）",{std::wstring(v.nameLines==0?i18n::Tr(L"隐藏"):v.nameLines==1?i18n::Tr(L"一行"):i18n::Tr(L"两行"))}),[this]{auto& m=View();m.nameLines=m.nameLines==0?2:m.nameLines-1;Refresh();owner.Save();}));
 {MenuFlyoutSubItem colors;colors.Text(i18n::Tr(L"主题色"));static wchar_t const* names[8]={L"红色",L"橙色",L"黄色",L"绿色",L"青色",L"蓝色",L"紫色",L"粉色"};
  auto swatchIcon=[](unsigned rgb){FontIcon ic;ic.Glyph(L"\u25A0");ic.FontFamily(FontFamily(L"Segoe UI Symbol"));ic.FontSize(16);ic.Foreground(SolidColorBrush(Windows::UI::Color{255,static_cast<uint8_t>((rgb>>16)&0xFF),static_cast<uint8_t>((rgb>>8)&0xFF),static_cast<uint8_t>(rgb&0xFF)}));return ic;};
  {MenuFlyoutItem none;none.Text(i18n::Tr(L"无"));none.Click([this](auto&&,auto&&){View().color=0;Refresh();owner.Save();});colors.Items().Append(none);}
  for(int k=1;k<=ZoneColorCount();++k){MenuFlyoutItem it;it.Text(i18n::Tr(names[k-1]));it.Icon(swatchIcon(ZoneColorRGB(k)));auto cc=k;it.Click([this,cc](auto&&,auto&&){View().color=cc;Refresh();owner.Save();});colors.Items().Append(it);}
  menu.Items().Append(colors);}
 menu.Items().Append(MenuItem(v.background.empty()?i18n::Tr(L"设置背景图…"):i18n::Tr(L"更换背景图…"),[this]{auto picked=shell::Pick(hwnd,false);if(picked.empty())return;if(!IsImagePath(picked.front())){Notify(i18n::Tr(L"请选择图片文件（png / jpg / bmp / gif / webp / tif）"));return;}View().background=picked.front();ApplyBackground();owner.Save();}));
 if(!v.background.empty()){menu.Items().Append(MenuItem(i18n::TrF(L"背景明暗：{0}（点击切换）",{std::wstring(i18n::Tr(v.dim==0?L"无":v.dim==1?L"适中":L"较暗"))}),[this]{auto& m=View();m.dim=(m.dim+1)%3;ApplyBackground();owner.Save();}));
 menu.Items().Append(MenuItem(i18n::Tr(L"清除背景图"),[this]{View().background.clear();ApplyBackground();owner.Save();}));}
 menu.Items().Append(MenuItem(i18n::TrF(L"透明度：{0}%（点击切换）",{std::to_wstring(v.opacity)}),[this]{auto& m=View();m.opacity=NextOpacityStep(m.opacity);root.Opacity(static_cast<double>(m.opacity)/100.0);owner.Save();Notify(i18n::TrF(L"透明度已设为 {0}%。",{std::to_wstring(m.opacity)}));}));
 {auto tick=[](){FontIcon f;f.FontFamily(FontFamily(L"Segoe Fluent Icons"));f.Glyph(L"\uE73E");f.FontSize(14);return f;};
  MenuFlyoutSubItem dir;dir.Text(i18n::Tr(L"展开方向"));
  for(int k=0;k<4;++k){MenuFlyoutItem it;it.Text(i18n::Tr(ExpandDirName(k)));if(z.expandDir==k)it.Icon(tick());auto dd=k;it.Click([this,dd](auto&&,auto&&){auto& m=Model();m.expandDir=ClampExpandDir(dd);Refresh();owner.Save();Notify(i18n::TrF(L"展开方向已设为「{0}」。",{std::wstring(i18n::Tr(ExpandDirName(dd)))}));});dir.Items().Append(it);}
  menu.Items().Append(dir);
  MenuFlyoutSubItem mh;mh.Text(i18n::Tr(L"展开高度上限"));
  auto addH=[&](wchar_t const* label,int pct){MenuFlyoutItem it;it.Text(i18n::Tr(label));if(z.maxHeight==pct)it.Icon(tick());it.Click([this,pct](auto&&,auto&&){auto& m=Model();int was=m.maxHeight,now=ClampMaxHeight(pct);if(was==now)return;m.maxHeight=now;RECT wk=WorkRect();int work=wk.bottom-wk.top;Reanchor(ZoneExpandedHeight(m.height,was,work),ZoneExpandedHeight(m.height,now,work));Place();Refresh();owner.Save();std::wstring shown=pct==0?std::wstring(i18n::Tr(L"不限")):std::to_wstring(pct)+L"%";Notify(i18n::TrF(L"展开高度上限已设为 {0}。",{shown}));});mh.Items().Append(it);};
  addH(L"不限",0);addH(L"25%",25);addH(L"50%",50);addH(L"75%",75);addH(L"100%",100);menu.Items().Append(mh);
  ToggleMenuFlyoutItem lk;lk.Text(i18n::Tr(L"锁定分区（禁止移动 / 缩放 / 删除）"));lk.IsChecked(z.locked);lk.Click([this](auto&&,auto&&){ToggleLock();});menu.Items().Append(lk);}
 menu.Items().Append(MenuItem(z.capsule?i18n::Tr(L"关闭胶囊模式"):i18n::Tr(L"胶囊模式（悬停展开）"),[this]{auto key=id;root.DispatcherQueue().TryEnqueue([this,key]{SetCapsule(!Model().capsule);Place();owner.Save();});}));
 if(z.group.empty()){MenuFlyoutSubItem merge;merge.Text(i18n::Tr(L"合并到标签组…"));for(auto const& o:owner.layout.zones){if(o.id==id)continue;merge.Items().Append(MenuItem(o.name,[this,tid=o.id]{auto key=id;root.DispatcherQueue().TryEnqueue([this,key,tid]{owner.MergeInto(key,tid);});}));}if(merge.Items().Size()>0)menu.Items().Append(merge);}
 else{MenuFlyoutSubItem join;join.Text(i18n::Tr(L"把其他分区并入此组…"));for(auto const& o:owner.layout.zones){if(o.group==z.group)continue;join.Items().Append(MenuItem(o.name,[this,tid=o.id]{auto key=id;root.DispatcherQueue().TryEnqueue([this,key,tid]{owner.MergeInto(key,tid);});}));}if(join.Items().Size()>0)menu.Items().Append(join);menu.Items().Append(MenuItem(i18n::TrF(L"把「{0}」移出标签组",{v.name}),[this]{auto key=viewId;root.DispatcherQueue().TryEnqueue([this,key]{owner.Ungroup(key);});}));}
 menu.Items().Append(MenuItem(i18n::Tr(L"新增分区"),[this]{owner.Add();}));
 menu.Items().Append(MenuItem(i18n::Tr(L"立即刷新"),[this]{if(!View().mappedFolder.empty())SyncMapped(View());Refresh();}));
 menu.Items().Append(MenuItem(owner.desktopMode?i18n::Tr(L"切换普通窗口"):i18n::Tr(L"试验桌面嵌入"),[this]{owner.ToggleDesktop();}));menu.Items().Append(MenuItem(i18n::Tr(L"设置"),[this]{owner.ShowSettings();}));menu.Items().Append(MenuItem(i18n::Tr(L"删除分区（保留原文件）"),[this]{if(Model().locked){Notify(i18n::Tr(L"分区已锁定：解锁后才能删除分区。"));return;}auto key=viewId;root.DispatcherQueue().TryEnqueue([this,key]{owner.Remove(key);});}));keepCapsuleOpen(this,menu);menu.ShowAt(target);}
void DeskWindow::EntryMenu(std::wstring const& path,std::wstring const& key,std::wstring const& stackId){
 auto& v=View();
 std::vector<std::wstring> custom;
 for(auto const& s:v.stacks)custom.push_back(i18n::TrF(L"移入叠放 · {0}",{s.name}));
 custom.push_back(i18n::Tr(L"新建叠放…"));
 if(!stackId.empty())custom.push_back(i18n::Tr(L"移出叠放"));
 custom.push_back(i18n::Tr(L"重新定位文件"));
 custom.push_back(i18n::Tr(L"移除入口（保留原文件）"));
 bool batch=IsSel(path)&&selected.size()>1;
 if(batch)custom.push_back(i18n::TrF(L"批量移除 {0} 个入口（保留原文件）",{std::to_wstring(selected.size())}));
 menuOpen=true;auto picked=shell::EntryContextMenu(hwnd,path,custom);menuOpen=false;
 if(picked<0){Refresh();return;}
 size_t idx=static_cast<size_t>(picked);
 if(idx<v.stacks.size()){AssignStack(v,key,v.stacks[idx].id);Refresh();owner.Save();return;}
 idx-=v.stacks.size();
 if(idx==0){auto sid=CreateStack(v);AssignStack(v,key,sid);expandedStack.clear();Refresh();owner.Save();return;}
 --idx;
 if(!stackId.empty()){if(idx==0){AssignStack(v,key,L"");Refresh();owner.Save();return;}--idx;}
 if(idx==0){
  auto relocated=shell::Pick(hwnd);
  if(relocated.empty())return;
  auto duplicate=std::find_if(v.entries.begin(),v.entries.end(),[&](auto const& x){return x.id!=key&&PathKey(x.path)==PathKey(relocated.front());});
  if(duplicate!=v.entries.end()){Notify(i18n::Tr(L"此目标已在当前分区中。 "));return;}
  for(auto& x:v.entries)if(x.id==key)x.path=relocated.front();
  Refresh();owner.Save();return;
 }
 if(idx==1){owner.PushUndo(i18n::TrF(L"移除入口「{0}」",{shell::Name(path)}));std::erase_if(v.entries,[&](auto const& x){return x.id==key;});Refresh();owner.Save();return;}
 if(idx==2){owner.PushUndo(i18n::TrF(L"批量移除 {0} 个入口",{std::to_wstring(selected.size())}));size_t n=0;for(auto const& k:selected){size_t before=v.entries.size();std::erase_if(v.entries,[&](auto const& x){return PathKey(x.path)==k;});n+=before-v.entries.size();}selected.clear();Refresh();owner.Save();Notify(i18n::TrF(L"已批量移除 {0} 个入口。",{std::to_wstring(n)}));return;}
}
void DeskWindow::Refresh(){auto& z=Model();auto& v=View();bool bodyFocus=listHost.FocusState()!=FocusState::Unfocused||grid.FocusState()!=FocusState::Unfocused;if(z.group.empty())viewId=z.id;if(lockBtn)lockBtn.Visibility(z.locked?Visibility::Visible:Visibility::Collapsed);root.Opacity(static_cast<double>(v.opacity)/100.0);if(capsuleNow){barGrid.Visibility(Visibility::Collapsed);grid.Visibility(Visibility::Collapsed);listHost.Visibility(Visibility::Collapsed);status.Visibility(Visibility::Collapsed);tabsPanel.Visibility(Visibility::Collapsed);if(crumbBar)crumbBar.Visibility(Visibility::Collapsed);pinBar.Visibility(Visibility::Collapsed);pill.Visibility(Visibility::Visible);unsigned cp=z.color?ZoneColorRGB(z.color):0;pill.Background(cp?Brush(SolidColorBrush(Windows::UI::Color{176,(uint8_t)((cp>>16)&0xFF),(uint8_t)((cp>>8)&0xFF),(uint8_t)(cp&0xFF)})):ThemeBrush(L"CardBackgroundFillColorDefault",Windows::UI::Color{255,60,60,60}));pillName.Text(z.name);window.Title(L"GuoDesk · "+z.name);bool has=!z.entries.empty();pillIcon.Visibility(has?Visibility::Visible:Visibility::Collapsed);pillGlyph.Visibility(has?Visibility::Collapsed:Visibility::Visible);if(has)shell::LoadIcon(z.entries.front().path,pillIcon);return;}RebuildTabs();barGrid.Visibility(Visibility::Visible);unsigned ht=v.color?ZoneColorRGB(v.color):0;barGrid.Background(ht?Brush(SolidColorBrush(Windows::UI::Color{64,(uint8_t)((ht>>16)&0xFF),(uint8_t)((ht>>8)&0xFF),(uint8_t)(ht&0xFF)})):Brush{nullptr});status.Visibility(Visibility::Visible);if(title.FocusState()==FocusState::Unfocused)title.Text(v.name);window.Title(L"GuoDesk · "+v.name);grid.Visibility(z.collapsed||v.viewMode!=L"grid"?Visibility::Collapsed:Visibility::Visible);listHost.Visibility(z.collapsed||v.viewMode!=L"list"?Visibility::Collapsed:Visibility::Visible);FontIcon chevronGlyph;chevronGlyph.FontFamily(FontFamily(L"Segoe Fluent Icons"));chevronGlyph.Glyph(z.collapsed?L"\uE70D":L"\uE70E");chevronGlyph.FontSize(16);chevron.Content(chevronGlyph);grid.Items().Clear();listPanel.Children().Clear();bool compact=owner.layout.settings.compact;bool mapped=!v.mappedFolder.empty();
 std::error_code bec;
 if(mapped&&(!v.browseInPlace||(!v.browseFolder.empty()&&(!UnderRoot(v.mappedFolder,v.browseFolder)||!std::filesystem::is_directory(std::filesystem::path(v.browseFolder),bec)))))v.browseFolder.clear();
 std::vector<Entry> browseItems;if(mapped&&!v.browseFolder.empty())browseItems=ListMapped(v.browseFolder);
 std::vector<Entry> const& items=mapped&&!v.browseFolder.empty()?browseItems:v.entries;
 size_t const limit=500,total=items.size();std::erase_if(selected,[&](auto const& k){return !std::any_of(items.begin(),items.end(),[&](auto const& e){return PathKey(e.path)==k;});});
 navPaths.clear();navVis.clear();
 auto itemMenu=[this](std::wstring const& path){MenuFlyout menu;auto& m=View();if(!m.browseFolder.empty())menu.Items().Append(MenuItem(i18n::Tr(L"返回上一级"),[this]{auto& q=View();Navigate(CrumbParent(q.mappedFolder,q.browseFolder));}));menu.Items().Append(MenuItem(i18n::Tr(L"打开"),[this,path]{TryOpen(path);}));menu.Items().Append(MenuItem(i18n::Tr(L"快速预览"),[this,path]{PreviewPath(path);}));menu.Items().Append(MenuItem(i18n::Tr(L"定位原文件"),[this,path]{TryReveal(path);}));
 menu.Items().Append(MenuItem(i18n::Tr(L"重命名…"),[this,path]{selected={PathKey(path)};RenameOne();}));
 menu.Items().Append(MenuItem(i18n::Tr(L"删除（回收站）…"),[this,path]{selected={PathKey(path)};DeleteSelected(false);}));keepCapsuleOpen(this,menu);return menu;};
 std::map<std::wstring,size_t> firstIdx;for(size_t i=0;i<v.entries.size();++i)if(!v.entries[i].stack.empty())firstIdx.emplace(v.entries[i].stack,i);
 std::set<std::wstring> rendered;
 auto renderCollapse=[&](std::wstring const& sid){int tier=v.tileSize-(compact?1:0);tier=std::clamp(tier,0,2);static int const TW[3]={72,88,112},TH[3]={78,94,118};Border tile;tile.Width(TW[tier]);tile.Height(TH[tier]);tile.CornerRadius(CornerRadius{8,8,8,8});tile.Background(ThemeBrush(L"CardBackgroundFillColorSecondary",Windows::UI::Color{255,80,80,80}));tile.AllowDrop(!mapped);StackPanel c;c.VerticalAlignment(VerticalAlignment::Center);c.Spacing(4);FontIcon g;g.FontFamily(FontFamily(L"Segoe Fluent Icons"));g.Glyph(L"\uE70E");g.FontSize(20);TextBlock t;t.Text(i18n::Tr(L"收起叠放"));t.FontSize(ScaledFont(owner.layout.settings.textSize,12));t.TextAlignment(TextAlignment::Center);t.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,180,180,180}));c.Children().Append(g);c.Children().Append(t);tile.Child(c);tile.DoubleTapped([this,sid](auto&&,auto&&){expandedStack.clear();Refresh();});tile.DragOver([mapped](auto&&,DragEventArgs const& a){a.AcceptedOperation(mapped?DataPackageOperation::None:(a.DataView().Contains(StandardDataFormats::StorageItems())?DataPackageOperation::Link:DataPackageOperation::Move));a.Handled(true);});if(!mapped)tile.Drop([this,sid](auto&&,DragEventArgs const& a){a.Handled(true);Drop(a,0,sid);});grid.Items().Append(tile);};
 auto renderPile=[&](std::wstring const& sid,size_t at){
  auto first=std::find_if(v.entries.begin(),v.entries.end(),[&](auto const& x){return x.stack==sid;});auto path=first->path;int count=StackCount(v,sid);bool exists=GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES;
  int tier=v.tileSize-(compact?1:0);tier=std::clamp(tier,0,2);static int const TW[3]={72,88,112},TH[3]={78,94,118},TI[3]={32,40,56};
  Grid wrap;wrap.Width(TW[tier]+8);wrap.Height(TH[tier]+8);
  Border back;back.Width(TW[tier]);back.Height(TH[tier]);back.CornerRadius(CornerRadius{8,8,8,8});back.Background(ThemeBrush(L"CardBackgroundFillColorSecondary",Windows::UI::Color{255,80,80,80}));back.Opacity(0.6);back.HorizontalAlignment(HorizontalAlignment::Right);back.VerticalAlignment(VerticalAlignment::Bottom);
  Border front;front.Width(TW[tier]);front.Height(TH[tier]);front.Padding(Thickness{4,4,4,4});front.CornerRadius(CornerRadius{8,8,8,8});front.Background(ThemeBrush(L"CardBackgroundFillColorDefault",Windows::UI::Color{255,60,60,60}));front.HorizontalAlignment(HorizontalAlignment::Left);front.VerticalAlignment(VerticalAlignment::Top);front.AllowDrop(!mapped);ToolTipService::SetToolTip(front,box_value(i18n::TrF(L"叠放 · {0} 项（双击展开）",{std::to_wstring(count)})));
  StackPanel content;content.Spacing(5);Image icon;icon.Width(TI[tier]);icon.Height(TI[tier]);content.Children().Append(icon);shell::LoadIcon(path,icon);TextBlock label;label.Text((exists?L"":L"⚠ ")+shell::Name(path));label.FontSize(ScaledFont(owner.layout.settings.textSize,12));label.TextAlignment(TextAlignment::Center);label.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,180,180,180}));if(v.nameLines==0)label.Visibility(Visibility::Collapsed);else if(v.nameLines==1){label.TextWrapping(TextWrapping::NoWrap);label.TextTrimming(TextTrimming::CharacterEllipsis);label.MaxHeight(18);}else{label.TextWrapping(TextWrapping::Wrap);label.MaxHeight(36);}content.Children().Append(label);front.Child(content);
  Border badge;badge.Width(20);badge.Height(20);badge.CornerRadius(CornerRadius{10,10,10,10});badge.Background(SolidColorBrush(Windows::UI::Color{255,0,120,212}));badge.HorizontalAlignment(HorizontalAlignment::Right);badge.VerticalAlignment(VerticalAlignment::Top);badge.Margin(Thickness{0,0,3,0});TextBlock cnt;cnt.Text(std::to_wstring(count));cnt.FontSize(11);cnt.Foreground(SolidColorBrush(Windows::UI::Colors::White()));cnt.HorizontalAlignment(HorizontalAlignment::Center);cnt.VerticalAlignment(VerticalAlignment::Center);badge.Child(cnt);
  wrap.Children().Append(back);wrap.Children().Append(front);wrap.Children().Append(badge);
  MenuFlyout pmenu;pmenu.Items().Append(MenuItem(i18n::Tr(L"展开叠放"),[this,sid]{expandedStack=sid;Refresh();}));
  pmenu.Items().Append(MenuItem(i18n::Tr(L"重命名叠放…"),[this,sid]{TextBox box;for(auto const& s:View().stacks)if(s.id==sid)box.Text(s.name);ContentDialog dlg;dlg.Title(box_value(i18n::Tr(L"重命名叠放")));dlg.PrimaryButtonText(i18n::Tr(L"保存"));dlg.CloseButtonText(i18n::Tr(L"取消"));dlg.DefaultButton(ContentDialogButton::Primary);StackPanel p;p.Spacing(8);p.MaxWidth(300);p.Children().Append(box);dlg.Content(p);try{dlg.XamlRoot(root.XamlRoot());}catch(...){return;}auto op=dlg.ShowAsync();op.Completed([this,sid,box](auto&&async,auto&&){if(async.GetResults()!=ContentDialogResult::Primary)return;auto t=std::wstring(box.Text());size_t a=t.find_first_not_of(L" \t");if(a==std::wstring::npos)return;t=t.substr(a,t.find_last_not_of(L" \t")-a+1);for(auto& s:View().stacks)if(s.id==sid)s.name=t;Refresh();owner.Save();});}));
  pmenu.Items().Append(MenuItem(i18n::Tr(L"解散叠放"),[this,sid]{std::wstring sname;for(auto const& s:View().stacks)if(s.id==sid)sname=s.name;owner.PushUndo(i18n::TrF(L"解散叠放「{0}」",{sname}));DissolveStack(View(),sid);Refresh();owner.Save();}));
  keepCapsuleOpen(this,pmenu);front.ContextFlyout(pmenu);
  front.PointerPressed([this](auto&&,Input::PointerRoutedEventArgs const&){FocusBody();});
  front.DoubleTapped([this,sid](auto&&,auto&&){expandedStack=sid;Refresh();});
  if(!mapped)front.DragStarting([this,sid,vkey=v.id](auto&&,DragStartingEventArgs const& a){a.Data().SetText(L"guodesk-stack:"+vkey+L"|"+sid);a.Data().RequestedOperation(DataPackageOperation::Move);});
  front.DragOver([this,mapped](auto&&,DragEventArgs const& a){if(mapped)OnZoneDragOver(a);else a.AcceptedOperation(a.DataView().Contains(StandardDataFormats::StorageItems())?DataPackageOperation::Link:DataPackageOperation::Move);a.Handled(true);});
  front.Drop([this,mapped,at,sid](auto&&,DragEventArgs const& a){a.Handled(true);if(mapped){OnZoneDrop(a);return;}Drop(a,at,sid);});
  grid.Items().Append(wrap);RegisterNav(path,front,false);};
 for(size_t i=0;i<std::min(total,limit);++i){auto const& e=items[i];auto path=e.path,key=e.id;auto index=i;bool exists=GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES;
  if(v.viewMode!=L"list"&&!e.stack.empty()&&expandedStack!=e.stack){if(!rendered.count(e.stack)){renderPile(e.stack,i);rendered.insert(e.stack);}continue;}
  if(v.viewMode!=L"list"&&!e.stack.empty()&&expandedStack==e.stack&&firstIdx[e.stack]==i)renderCollapse(e.stack);
  if(v.viewMode==L"list"){
   Border row;row.Height(30);row.CornerRadius(CornerRadius{4,4,4,4});row.Padding(Thickness{8,3,8,3});row.Background(ItemFill(path));ToolTipService::SetToolTip(row,box_value(path));row.PointerEntered([weak=make_weak(row),this](auto&&,auto&&){if(auto r=weak.get())r.Background(ThemeBrush(L"CardBackgroundFillColorSecondary",Windows::UI::Color{255,80,80,80}));});row.PointerExited([weak=make_weak(row),path,this](auto&&,auto&&){if(auto r=weak.get())r.Background(ItemFill(path));});row.Tapped([this,path](auto&&,Input::TappedRoutedEventArgs const& t){t.Handled(true);TapSelect(path);});
   Grid line;ColumnDefinition ci,cn,cp;ci.Width(GridLength{0,GridUnitType::Auto});cn.Width(GridLength{2,GridUnitType::Star});cp.Width(GridLength{3,GridUnitType::Star});line.ColumnDefinitions().Append(ci);line.ColumnDefinitions().Append(cn);line.ColumnDefinitions().Append(cp);
   Image icon;icon.Width(18);icon.Height(18);line.Children().Append(icon);shell::LoadIcon(path,icon);
   TextBlock name;name.Text((exists?L"":L"⚠ ")+shell::Name(path));name.FontSize(ScaledFont(owner.layout.settings.textSize,12));name.VerticalAlignment(VerticalAlignment::Center);name.Margin(Thickness{8,0,8,0});name.TextTrimming(TextTrimming::CharacterEllipsis);name.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,180,180,180}));Grid::SetColumn(name,1);line.Children().Append(name);
   TextBlock where;where.Text(path);where.FontSize(11);where.VerticalAlignment(VerticalAlignment::Center);where.TextTrimming(TextTrimming::CharacterEllipsis);where.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,160,160,160}));Grid::SetColumn(where,2);line.Children().Append(where);
   row.Child(line);row.DoubleTapped([this,path](auto&&,auto&&){OpenFocused(path);});if(mapped)row.ContextFlyout(itemMenu(path));else row.ContextRequested([this,path,key,sid=e.stack](auto&&,auto&& a){a.Handled(true);EntryMenu(path,key,sid);});AttachDrag(row,path);listPanel.Children().Append(row);RegisterNav(path,row,true);
  }else{
   int tier=v.tileSize-(compact?1:0);tier=std::clamp(tier,0,2);static int const TW[3]={72,88,112},TH[3]={78,94,118},TI[3]={32,40,56};
   Border tile;tile.Width(TW[tier]);tile.Height(TH[tier]);tile.Padding(Thickness{4,4,4,4});tile.CornerRadius(CornerRadius{8,8,8,8});tile.Background(ItemFill(path));tile.CanDrag(!mapped);tile.AllowDrop(true);ToolTipService::SetToolTip(tile,box_value(path));tile.PointerEntered([weak=make_weak(tile),this](auto&&,auto&&){if(auto t=weak.get())t.Background(ThemeBrush(L"CardBackgroundFillColorSecondary",Windows::UI::Color{255,80,80,80}));});tile.PointerExited([weak=make_weak(tile),path,this](auto&&,auto&&){if(auto t=weak.get())t.Background(ItemFill(path));});tile.Tapped([this,path](auto&&,Input::TappedRoutedEventArgs const& t){t.Handled(true);TapSelect(path);});StackPanel content;content.Spacing(5);Image icon;icon.Width(TI[tier]);icon.Height(TI[tier]);content.Children().Append(icon);shell::LoadIcon(path,icon);TextBlock label;label.Text((exists?L"":L"⚠ ")+shell::Name(path));label.FontSize(ScaledFont(owner.layout.settings.textSize,12));label.TextAlignment(TextAlignment::Center);label.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,180,180,180}));if(v.nameLines==0)label.Visibility(Visibility::Collapsed);else if(v.nameLines==1){label.TextWrapping(TextWrapping::NoWrap);label.TextTrimming(TextTrimming::CharacterEllipsis);label.MaxHeight(18);}else{label.TextWrapping(TextWrapping::Wrap);label.MaxHeight(36);}content.Children().Append(label);tile.Child(content);
   tile.DoubleTapped([this,path](auto&&,auto&&){OpenFocused(path);});
   if(!mapped)tile.DragStarting([key](auto&&,DragStartingEventArgs const& a){a.Data().SetText(L"guodesk-entry:"+key);a.Data().RequestedOperation(DataPackageOperation::Move);});
   tile.DragOver([this,mapped](auto&&,DragEventArgs const& a){if(mapped)OnZoneDragOver(a);else a.AcceptedOperation(a.DataView().Contains(StandardDataFormats::StorageItems())?DataPackageOperation::Link:DataPackageOperation::Move);a.Handled(true);});
   tile.Drop([this,mapped,index,sid=e.stack](auto&&,DragEventArgs const& a){a.Handled(true);if(mapped){OnZoneDrop(a);return;}Drop(a,index,sid);});
   if(mapped)tile.ContextFlyout(itemMenu(path));else tile.ContextRequested([this,path,key,sid=e.stack](auto&&,auto&& a){a.Handled(true);EntryMenu(path,key,sid);});AttachDrag(tile,path);grid.Items().Append(tile);RegisterNav(path,tile,true);}
 }
 if(focusIdx>=static_cast<int>(navPaths.size()))focusIdx=static_cast<int>(navPaths.size())-1;
 RepaintNav();
 RenderCrumbs();
 if(mapped){std::wstring here=v.browseFolder.empty()?v.mappedFolder:v.browseFolder;std::wstring head=total==0?i18n::Tr(L"映射文件夹为空或不可访问"):i18n::TrF(L"映射视图（只读）· {0} · 共 {1} 项",{shell::Name(here),std::to_wstring(total)});if(total>limit)head+=i18n::TrF(L" · 已显示前 {0} 项",{std::to_wstring(limit)});if(v.browseInPlace)head+=i18n::Tr(L" · 双击文件夹可进入");Notify(head+i18n::Tr(L" · 右键更多"));}
 else Notify(v.entries.empty()?i18n::Tr(L"拖入文件、文件夹或应用快捷方式 · 原文件保持原位"):selected.empty()?i18n::Tr(L"双击打开 · 右键管理 · 拖拽排序 · 单击后方向键选择"):i18n::TrF(L"已选 {0} 项 · 拖出包含全部选中",{std::to_wstring(selected.size())}));
 RebuildPins();if(bodyFocus)FocusBody();}
// Shift 状态要在 co_await 之前读：await 会让出 UI 线程，用户可能在续跑前就松开了键
fire_and_forget DeskWindow::Drop(DragEventArgs a,size_t position,std::wstring stackId){auto deferral=a.GetDeferral();auto weak=winrt::make_weak(root);auto key=viewId;auto* controller=&owner;auto dest=TargetFolder();bool mapped=!dest.empty();int const op=DropOperation(mapped,(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0);bool move=(op==2);try{auto data=a.DataView();auto zoneOf=[&](std::wstring const& kid)->Zone*{auto it=std::find_if(controller->layout.zones.begin(),controller->layout.zones.end(),[&](auto const& z){return z.id==kid;});return it==controller->layout.zones.end()?nullptr:&*it;};if(data.Contains(StandardDataFormats::StorageItems())){auto items=co_await data.GetStorageItemsAsync();if(mapped){std::vector<std::wstring> paths;for(auto const& item:items)if(!item.Path().empty())paths.push_back(std::wstring(item.Path()));if(!paths.empty())if(auto live=weak.get())live.DispatcherQueue().TryEnqueue([this,guard=alive,paths=std::move(paths),dest,move]{if(!*guard)return;DropIntoFolder(paths,dest,move);});else if(items.Size())Notify(i18n::Tr(L"拖入的项目不在磁盘上，无法放入映射文件夹。"));}else if(auto zt=zoneOf(key))for(auto const& item:items)if(!item.Path().empty()){auto pk=PathKey(std::wstring(item.Path()));if(AddEntry(*zt,std::wstring(item.Path()))&&!stackId.empty())for(auto& e:zt->entries)if(PathKey(e.path)==pk)e.stack=stackId;}}else if(data.Contains(StandardDataFormats::Text())){std::wstring text(co_await data.GetTextAsync());if(text.starts_with(L"guodesk-entry:")){auto eid=text.substr(14);controller->MoveEntry(eid,key,position);if(!stackId.empty())if(auto zt=zoneOf(key))AssignStack(*zt,eid,stackId);}else if(text.starts_with(L"guodesk-stack:")){auto rest=text.substr(14);auto bar=rest.find(L'|');if(bar!=std::wstring::npos)MoveStack(controller->layout,rest.substr(bar+1),rest.substr(0,bar),key);}}if(auto live=weak.get())live.DispatcherQueue().TryEnqueue([controller]{controller->Refresh();controller->Save();});}catch(...){if(auto live=weak.get())MessageBoxW(nullptr,i18n::Tr(L"无法添加拖入项目。").c_str(),L"GuoDesk",MB_OK);}deferral.Complete();}
void DeskWindow::OnZoneDragOver(DragEventArgs const& a){
 // 映射分区只接受文件负载（真实落盘），普通分区保持"拖入=按引用加入口"的语义
 auto data=a.DataView();bool storage=data.Contains(StandardDataFormats::StorageItems());
 if(!View().mappedFolder.empty()){
  int const op=DropOperation(true,(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0);
  a.AcceptedOperation(!storage?DataPackageOperation::None:(op==2?DataPackageOperation::Move:DataPackageOperation::Copy));
 }else a.AcceptedOperation(storage?DataPackageOperation::Link:DataPackageOperation::Move);
 a.Handled(true);
}
void DeskWindow::OnZoneDrop(DragEventArgs const& a){a.Handled(true);Drop(a,View().entries.size());}
void DeskWindow::DropIntoFolder(std::vector<std::wstring> const& paths,std::wstring const& dest,bool move){
 if(paths.empty()||dest.empty())return;
 if(opRunning){Notify(i18n::Tr(L"上一个文件操作还在进行，请稍候或点击“取消”。"));return;}
 long long const total=static_cast<long long>(paths.size());
 auto res=std::make_shared<shell::TransferResult>();
 BeginOp(i18n::TrF(move?L"正在移动 {0} 项到映射文件夹…":L"正在复制 {0} 项到映射文件夹…",{std::to_wstring(total)}),
  [paths,dest,move,res](shell::CancelFlag const& cancel,shell::ProgressFn const& prog){*res=shell::TransferFiles(paths,dest,move,cancel,prog);},
  [this,guard=alive,res,dest,total,move]{
   if(!*guard)return;
   long long const ok=static_cast<long long>(res->made.size());
   auto const state=OpOutcome(ok,res->failed,res->cancelled);
   std::wstring text;
   if(state==3)text=i18n::Tr(move?L"移动未完成：文件可能被占用或目标文件夹不可写。":L"复制未完成：文件可能被占用或目标文件夹不可写。");
   else if(state==4)text=i18n::Tr(move?L"已取消移动，未移动任何文件。":L"已取消复制，未复制任何文件。");
   else if(!ok)text=i18n::Tr(move?L"没有文件被移动（项目已在目标文件夹中）。":L"没有新增副本（项目已在目标文件夹中）。");
   else if(state==2)text=i18n::TrF(move?L"已移动 {0} 项，{1} 项失败。":L"已复制 {0} 项，{1} 项失败。",{std::to_wstring(ok),std::to_wstring(res->failed)});
   else text=i18n::TrF(move?L"已移动 {0} 项到「{1}」。":L"已复制 {0} 项到「{1}」。",{std::to_wstring(ok),shell::Name(dest)});
   if(ok&&res->skipped)text+=L" "+i18n::TrF(L"跳过 {0} 项（已在目标文件夹中）。",{std::to_wstring(res->skipped)});
   if(ok&&res->cancelled)text+=L" "+i18n::TrF(L"已取消剩余 {0} 项。",{std::to_wstring(std::max<long long>(0,total-ok-res->skipped-res->failed))});
   SyncMapped(View());Refresh();
   Notify(text);
  });
}
LRESULT CALLBACK DeskWindow::Subclass(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data){auto* self=reinterpret_cast<DeskWindow*>(data);if(!self->Exists())return DefSubclassProc(h,msg,w,l);if(msg==WM_NCCALCSIZE&&w&&self->desktop)return 0;if(msg==WM_NCHITTEST&&self->Model().locked){auto res=DefSubclassProc(h,msg,w,l);if(res>=HTLEFT&&res<=HTBOTTOMRIGHT)return HTCLIENT;return res;}if(msg==WM_ENTERSIZEMOVE){self->lockRevert=self->Model().locked;if(self->lockRevert)GetWindowRect(h,&self->lockRect);}if(msg==WM_SIZING&&l){auto* rr=reinterpret_cast<RECT*>(l);if(!self->Model().locked)self->SnapResize(*rr,static_cast<int>(w));self->CapResize(*rr,static_cast<int>(w));return TRUE;}if(msg==WM_EXITSIZEMOVE){if(self->lockRevert){self->lockRevert=false;SetWindowPos(h,nullptr,self->lockRect.left,self->lockRect.top,self->lockRect.right-self->lockRect.left,self->lockRect.bottom-self->lockRect.top,SWP_NOZORDER|SWP_NOACTIVATE);self->Notify(i18n::Tr(L"分区已锁定：位置与尺寸已还原。"));}self->Capture();self->owner.Save();}if(msg==WM_DISPLAYCHANGE)self->Place();if(msg==WM_DPICHANGED){auto* rect=reinterpret_cast<RECT*>(l);SetWindowPos(h,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);self->Capture();self->Place();self->owner.Save();}return DefSubclassProc(h,msg,w,l);}
}

namespace guodesk {
static std::wstring StampOf(std::wstring const& folder){WIN32_FILE_ATTRIBUTE_DATA fa{};if(!GetFileAttributesExW(folder.c_str(),GetFileExInfoStandard,&fa))return{};return std::to_wstring(fa.ftLastWriteTime.dwHighDateTime)+L"-"+std::to_wstring(fa.ftLastWriteTime.dwLowDateTime);}
static constexpr UINT WM_APP_CTRL_TOGGLE=WM_APP+4;
static HWND g_ctrlTarget{};
static DWORD g_lastCtrlDown=0;
static bool g_inCtrl=false,g_ctrlUpSeen=false,g_otherKey=false;
static LRESULT CALLBACK CtrlHookProc(int code,WPARAM w,LPARAM l){
 if(code>=0&&g_ctrlTarget){
  auto* info=reinterpret_cast<KBDLLHOOKSTRUCT*>(l);
  bool ctrl=info->vkCode==VK_LCONTROL||info->vkCode==VK_RCONTROL;
  if(w==WM_KEYDOWN||w==WM_SYSKEYDOWN){
   if(ctrl){
    if(!g_inCtrl){
     DWORD now=GetTickCount();
     if(g_ctrlUpSeen&&!g_otherKey&&g_lastCtrlDown&&now-g_lastCtrlDown<=450){
      g_lastCtrlDown=0;g_ctrlUpSeen=false;
      PostMessageW(g_ctrlTarget,WM_APP_CTRL_TOGGLE,0,0);
     }else g_lastCtrlDown=now;
     g_inCtrl=true;g_otherKey=false;g_ctrlUpSeen=false;
    }
   }else g_otherKey=true;
  }else if(w==WM_KEYUP||w==WM_SYSKEYUP){
   if(ctrl){g_inCtrl=false;g_ctrlUpSeen=true;}
  }
 }
 return CallNextHookEx(nullptr,code,w,l);
}
static HWND g_revealTarget{};
static Hotkey g_revealKey{},g_raiseKey{};
static bool RevealModsDown(unsigned mods){
 if((mods&MOD_CONTROL)&&!(GetAsyncKeyState(VK_CONTROL)&0x8000))return false;
 if((mods&MOD_ALT)&&!(GetAsyncKeyState(VK_MENU)&0x8000))return false;
 if((mods&MOD_SHIFT)&&!(GetAsyncKeyState(VK_SHIFT)&0x8000))return false;
 if((mods&MOD_WIN)&&!(GetAsyncKeyState(VK_LWIN)&0x8000||GetAsyncKeyState(VK_RWIN)&0x8000))return false;
 return true;
}
static LRESULT CALLBACK RevealHookProc(int code,WPARAM w,LPARAM l){
 if(code>=0&&g_revealTarget){
  auto* info=reinterpret_cast<KBDLLHOOKSTRUCT*>(l);
  auto feed=[&](Hotkey const& k,UINT down,UINT up){
   if(!k.vk||info->vkCode!=k.vk)return;
   if(w==WM_KEYDOWN||w==WM_SYSKEYDOWN){if(RevealModsDown(k.mods))PostMessageW(g_revealTarget,down,0,0);}
   else if(w==WM_KEYUP||w==WM_SYSKEYUP)PostMessageW(g_revealTarget,up,0,0);};
  feed(g_revealKey,WM_APP+5,WM_APP+6);
  feed(g_raiseKey,WM_APP+7,WM_APP+8);
 }
 return CallNextHookEx(nullptr,code,w,l);
}
Controller::Controller(std::filesystem::path root):store(std::move(root)){mutex=CreateMutexW(nullptr,FALSE,L"Local\\GuoDesk.Prototype");if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(mutex);mutex=nullptr;HWND prev=FindWindowW(L"GuoDesk.Controller",nullptr);if(prev)PostMessageW(prev,WM_APP+2,0,0);throw already_running{};}}
Controller::~Controller(){*syncAlive=false;RemoveCtrlHook();RemoveRevealHook();if(messageWindow){UnregisterHotKey(messageWindow,1);UnregisterHotKey(messageWindow,2);UnregisterHotKey(messageWindow,3);UnregisterHotKey(messageWindow,4);KillTimer(messageWindow,1);KillTimer(messageWindow,2);Shell_NotifyIconW(NIM_DELETE,&tray);DestroyWindow(messageWindow);}capture.reset();note.reset();todo.reset();clockW.reset();guide.reset();tidy.reset();settings.reset();windows.clear();if(mutex)CloseHandle(mutex);}
void Controller::AddTray(){tray={sizeof(tray)};tray.hWnd=messageWindow;tray.uID=1;tray.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;tray.uCallbackMessage=WM_APP+1;tray.hIcon=LoadIconW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(101));if(!tray.hIcon)tray.hIcon=LoadIconW(nullptr,IDI_APPLICATION);wcscpy_s(tray.szTip,i18n::Tr(L"GuoDesk · 桌面分区").c_str());Shell_NotifyIconW(NIM_ADD,&tray);}
void Controller::Start(){std::wstring warning;layout=store.Load(warning);bool topoSwapped=false;{auto sig=TopologySignature(EnumMonitorAreas());if(!sig.empty()&&sig!=layout.topologyLast){if(!layout.topologyLast.empty()){try{ArchiveTopology(layout.topologyArchives,layout.topologyLast,Serialize(layout));}catch(...){}}std::string restore;if(TakeTopology(layout.topologyArchives,sig,restore)){try{auto next=Deserialize(restore);next.settings=layout.settings;next.topologyArchives=layout.topologyArchives;next.topologyLast=sig;layout=std::move(next);topoSwapped=true;}catch(...){layout.topologyLast=sig;}}else layout.topologyLast=sig;}}i18n::SetLanguage(layout.settings.language);for(auto& z:layout.zones)if(!z.mappedFolder.empty())SyncMapped(z);for(auto& z:layout.zones){RECT r{z.x,z.y,z.x+z.width,z.y+z.height};Reanchor(r,z.mon,z.mx,z.my);z.x=r.left;z.y=r.top;}{auto& wg=layout.widgets;RECT rn{wg.noteX,wg.noteY,wg.noteX+wg.noteW,wg.noteY+wg.noteH};Reanchor(rn,wg.noteMon,wg.noteMX,wg.noteMY);wg.noteX=rn.left;wg.noteY=rn.top;RECT rt{wg.todoX,wg.todoY,wg.todoX+wg.todoW,wg.todoY+wg.todoH};Reanchor(rt,wg.todoMon,wg.todoMX,wg.todoMY);wg.todoX=rt.left;wg.todoY=rt.top;RECT rc{wg.clockX,wg.clockY,wg.clockX+wg.clockW,wg.clockY+wg.clockH};Reanchor(rc,wg.clockMon,wg.clockMX,wg.clockMY);wg.clockX=rc.left;wg.clockY=rc.top;RECT rm{wg.musicX,wg.musicY,wg.musicX+wg.musicW,wg.musicY+wg.musicH};Reanchor(rm,wg.musicMon,wg.musicMX,wg.musicMY);wg.musicX=rm.left;wg.musicY=rm.top;RECT rw{wg.weatherX,wg.weatherY,wg.weatherX+wg.weatherW,wg.weatherY+wg.weatherH};Reanchor(rw,wg.weatherMon,wg.weatherMX,wg.weatherMY);wg.weatherX=rw.left;wg.weatherY=rw.top;RECT rs{wg.searchX,wg.searchY,wg.searchX+560,wg.searchY+440};Reanchor(rs,wg.searchMon,wg.searchMX,wg.searchMY);wg.searchX=rs.left;wg.searchY=rs.top;RECT rag{wg.appGridX,wg.appGridY,wg.appGridX+wg.appGridW,wg.appGridY+wg.appGridH};Reanchor(rag,wg.appGridMon,wg.appGridMX,wg.appGridMY);wg.appGridX=rag.left;wg.appGridY=rag.top;}WNDCLASSW cls{};cls.lpfnWndProc=MessageProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"GuoDesk.Controller";RegisterClassW(&cls);messageWindow=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,L"GuoDesk Controller",WS_POPUP,0,0,0,0,nullptr,nullptr,cls.hInstance,this);taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");AddTray();for(auto const& z:layout.zones){if(!z.group.empty()&&GroupMemberIds(layout,z.group).front()!=z.id)continue;windows.push_back(std::make_unique<DeskWindow>(*this,z.id));if(!z.mappedFolder.empty()){auto stamp=StampOf(z.mappedFolder);if(!stamp.empty())mappedStamp[PathKey(z.mappedFolder)]=stamp;}}if(topoSwapped&&!windows.empty())windows.front()->Notify(i18n::Tr(L"检测到显示器变化，已恢复该显示器的布局。"));if(!ApplyHotkey()&&!windows.empty())windows.front()->Notify(i18n::Tr(L"全局热键注册失败，可能已被其他程序占用。"));SetTimer(messageWindow,1,2000,nullptr);if(layout.widgets.noteVisible)note=std::make_unique<NoteWindow>(*this);if(layout.widgets.todoVisible)todo=std::make_unique<TodoWindow>(*this);if(layout.widgets.clockVisible)clockW=std::make_unique<ClockWindow>(*this);if(layout.widgets.musicVisible)music=std::make_unique<MusicWindow>(*this);if(layout.widgets.weatherVisible)weather=std::make_unique<WeatherWindow>(*this);if(layout.widgets.appGridVisible)appGrid=std::make_unique<AppGridWindow>(*this);Save();if(!layout.settings.guideDone)ShowGuide();if(!warning.empty())MessageBoxW(nullptr,warning.c_str(),i18n::Tr(L"GuoDesk 配置恢复").c_str(),MB_OK|MB_ICONINFORMATION);}
void Controller::Save(){try{store.Save(layout);}catch(...){for(auto& w:windows)w->Notify(i18n::Tr(L"保存失败：请检查本地数据目录权限和剩余空间。 "));return;}if(layout.settings.syncAuto&&!layout.settings.syncUrl.empty()&&!layout.settings.syncPass.empty()&&messageWindow)SetTimer(messageWindow,2,20000,nullptr);}
std::string Controller::UndoMark(){try{return Serialize(layout);}catch(...){return {};}}
void Controller::UndoPush(std::wstring const& label,std::string mark){if(mark.empty())return;undo.Push(std::move(label),std::move(mark));}
void Controller::PushUndo(std::wstring const& label){UndoPush(label,UndoMark());}
void Controller::Undo(){
 UndoFrame frame;
 if(!undo.Pop(frame)){if(!windows.empty())windows.front()->Notify(i18n::Tr(L"没有可撤销的操作。"));return;}
 try{
  auto next=Deserialize(frame.snapshot);
  ImportLayout(std::move(next));
  if(!windows.empty())windows.front()->Notify(i18n::TrF(L"已撤销：{0}",{frame.label}));
 }catch(...){
  undo.Clear();
  MessageBoxW(nullptr,i18n::Tr(L"撤销失败：这一步的记录已失效，历史已清空。").c_str(),L"GuoDesk",MB_OK|MB_ICONERROR);
 }
}
void Controller::HealTopology(){
 auto sig=TopologySignature(EnumMonitorAreas());
 if(sig.empty()||sig==layout.topologyLast)return;
 std::string body;try{body=Serialize(layout);}catch(...){return;}
 if(!layout.topologyLast.empty())ArchiveTopology(layout.topologyArchives,layout.topologyLast,body);
 auto settings=layout.settings;auto archives=layout.topologyArchives;
 std::string restore;
 if(TakeTopology(archives,sig,restore)){try{auto next=Deserialize(restore);next.settings=settings;next.topologyArchives=archives;next.topologyLast=sig;ImportLayout(std::move(next),i18n::Tr(L"检测到显示器变化，已恢复该显示器的布局。").c_str());return;}catch(...){}}
 layout.topologyLast=sig;Save();
}
void Controller::SyncUploadAuto(){auto url=layout.settings.syncUrl;auto user=layout.settings.syncUser;auto pass=webdav::UnprotectSecret(layout.settings.syncPass);if(url.empty()||pass.empty())return;std::string data;try{data=Serialize(layout);}catch(...){return;}HWND mw=messageWindow;std::thread([mw,data=std::move(data),target=webdav::JoinUrl(url,L"guodesk-layout.json"),user,pass,insecure=layout.settings.syncInsecure](){if(webdav::UploadText(target,user,pass,data,insecure))return;PostMessageW(mw,WM_APP+3,0,0);}).detach();}
void Controller::ImportLayout(Layout&& next,wchar_t const* notice){tidy.reset();note.reset();todo.reset();clockW.reset();music.reset();weather.reset();appGrid.reset();try{store.Save(next);}catch(...){for(auto& w:windows)w->Notify(i18n::Tr(L"保存失败：请检查本地数据目录权限和剩余空间。 "));return;}layout=std::move(next);i18n::SetLanguage(layout.settings.language);mappedStamp.clear();for(auto& z:layout.zones)if(!z.mappedFolder.empty())SyncMapped(z);for(auto& z:layout.zones){RECT r{z.x,z.y,z.x+z.width,z.y+z.height};Reanchor(r,z.mon,z.mx,z.my);z.x=r.left;z.y=r.top;}{auto& wg=layout.widgets;RECT rn{wg.noteX,wg.noteY,wg.noteX+wg.noteW,wg.noteY+wg.noteH};Reanchor(rn,wg.noteMon,wg.noteMX,wg.noteMY);wg.noteX=rn.left;wg.noteY=rn.top;RECT rt{wg.todoX,wg.todoY,wg.todoX+wg.todoW,wg.todoY+wg.todoH};Reanchor(rt,wg.todoMon,wg.todoMX,wg.todoMY);wg.todoX=rt.left;wg.todoY=rt.top;RECT rc{wg.clockX,wg.clockY,wg.clockX+wg.clockW,wg.clockY+wg.clockH};Reanchor(rc,wg.clockMon,wg.clockMX,wg.clockMY);wg.clockX=rc.left;wg.clockY=rc.top;RECT rm{wg.musicX,wg.musicY,wg.musicX+wg.musicW,wg.musicY+wg.musicH};Reanchor(rm,wg.musicMon,wg.musicMX,wg.musicMY);wg.musicX=rm.left;wg.musicY=rm.top;RECT rw{wg.weatherX,wg.weatherY,wg.weatherX+wg.weatherW,wg.weatherY+wg.weatherH};Reanchor(rw,wg.weatherMon,wg.weatherMX,wg.weatherMY);wg.weatherX=rw.left;wg.weatherY=rw.top;RECT rs{wg.searchX,wg.searchY,wg.searchX+560,wg.searchY+440};Reanchor(rs,wg.searchMon,wg.searchMX,wg.searchMY);wg.searchX=rs.left;wg.searchY=rs.top;RECT rag{wg.appGridX,wg.appGridY,wg.appGridX+wg.appGridW,wg.appGridY+wg.appGridH};Reanchor(rag,wg.appGridMon,wg.appGridMX,wg.appGridMY);wg.appGridX=rag.left;wg.appGridY=rag.top;}SyncWindows();for(auto const& z:layout.zones)if(!z.mappedFolder.empty()){auto stamp=StampOf(z.mappedFolder);if(!stamp.empty())mappedStamp[PathKey(z.mappedFolder)]=stamp;}if(desktopMode)for(auto& w:windows)w->SetDesktop(true);if(layout.widgets.noteVisible)note=std::make_unique<NoteWindow>(*this);if(layout.widgets.todoVisible)todo=std::make_unique<TodoWindow>(*this);if(layout.widgets.clockVisible)clockW=std::make_unique<ClockWindow>(*this);if(layout.widgets.musicVisible)music=std::make_unique<MusicWindow>(*this);if(layout.widgets.weatherVisible)weather=std::make_unique<WeatherWindow>(*this);if(layout.widgets.appGridVisible)appGrid=std::make_unique<AppGridWindow>(*this);ApplySettings();for(auto& w:windows)w->Place();if(!windows.empty())windows.front()->Notify(i18n::Tr(notice));}
void Controller::Add(){PushUndo(i18n::Tr(L"新增分区"));Zone zone;zone.id=NewId();zone.name=i18n::Tr(L"新分区");zone.x+=static_cast<int>(layout.zones.size())*30;zone.y+=static_cast<int>(layout.zones.size())*30;auto key=zone.id;layout.zones.push_back(std::move(zone));windows.push_back(std::make_unique<DeskWindow>(*this,key));if(desktopMode)windows.back()->SetDesktop(true);Save();}
void Controller::QuickZone(std::wstring const& tag){RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);auto mark=UndoMark();if(AddQuickZone(layout,tag,work)){UndoPush(i18n::TrF(L"快速分区「{0}」",{KnownFolderName(tag)}),std::move(mark));SyncWindows();Save();}else if(!windows.empty())windows.front()->Notify(i18n::Tr(L"该文件夹已有对应分区。"));}
void Controller::UseTemplate(ZoneTemplate const& tpl){RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);auto mark=UndoMark();if(ApplyTemplate(layout,tpl,work)){UndoPush(i18n::TrF(L"应用模板「{0}」",{i18n::Tr(tpl.name)}),std::move(mark));SyncWindows();Save();}else if(!windows.empty())windows.front()->Notify(i18n::TrF(L"「{0}」的文件夹已全部有分区，无需重复添加。",{i18n::Tr(tpl.name)}));}
void Controller::SyncWindows(){std::vector<std::wstring> keep;for(auto const& z:layout.zones){if(!z.group.empty()&&GroupMemberIds(layout,z.group).front()!=z.id)continue;keep.push_back(z.id);}for(auto it=windows.begin();it!=windows.end();){if(std::find(keep.begin(),keep.end(),(*it)->id)==keep.end()){it=windows.erase(it);}else ++it;}for(auto const& kid:keep){bool has=false;for(auto const& w:windows)if(w->id==kid)has=true;if(!has){windows.push_back(std::make_unique<DeskWindow>(*this,kid));if(desktopMode)windows.back()->SetDesktop(true);}}}
void Controller::MergeInto(std::wstring const& selfId,std::wstring const& otherId){auto a=std::find_if(layout.zones.begin(),layout.zones.end(),[&](auto const& z){return z.id==selfId;});auto t=std::find_if(layout.zones.begin(),layout.zones.end(),[&](auto const& z){return z.id==otherId;});if(a==layout.zones.end()||t==layout.zones.end()||a==t)return;auto mark=UndoMark();std::wstring gid;if(!t->group.empty()&&t->group!=a->group){gid=t->group;a->group=gid;}else{gid=a->group.empty()?NewId():a->group;a->group=gid;t->group=gid;}auto members=GroupMemberIds(layout,gid);auto tabIndex=std::find(members.begin(),members.end(),a->id);if(tabIndex!=members.end())a->groupTab=static_cast<int>(tabIndex-members.begin());for(auto& z:layout.zones)if(z.group==gid)z.groupTab=std::clamp(z.groupTab,0,static_cast<int>(members.size())-1);UndoPush(i18n::TrF(L"并入标签组「{0}」",{a->name}),std::move(mark));SyncWindows();Save();}
void Controller::Ungroup(std::wstring const& zoneId){auto it=std::find_if(layout.zones.begin(),layout.zones.end(),[&](auto const& z){return z.id==zoneId;});if(it==layout.zones.end()||it->group.empty())return;auto gid=it->group;auto name=it->name;auto mark=UndoMark();it->group.clear();it->groupTab=0;if(GroupMemberIds(layout,gid).size()<=1)for(auto& z:layout.zones)if(z.group==gid){z.group.clear();z.groupTab=0;}UndoPush(i18n::TrF(L"移出标签组「{0}」",{name}),std::move(mark));SyncWindows();Save();}
void Controller::AddToGroup(std::wstring const& anchorId){auto a=std::find_if(layout.zones.begin(),layout.zones.end(),[&](auto const& z){return z.id==anchorId;});if(a==layout.zones.end())return;PushUndo(i18n::TrF(L"在「{0}」组新增分区",{a->name}));Zone nz;nz.id=NewId();nz.name=i18n::Tr(L"新分区");if(a->group.empty()){a->group=NewId();}nz.group=a->group;nz.x=a->x;nz.y=a->y;nz.width=a->width;nz.height=a->height;nz.mon=a->mon;nz.mx=a->mx;nz.my=a->my;nz.collapsed=a->collapsed;nz.expandDir=a->expandDir;nz.maxHeight=a->maxHeight;layout.zones.push_back(std::move(nz));auto members=GroupMemberIds(layout,a->group);for(auto& z:layout.zones)if(z.group==a->group)z.groupTab=std::clamp(z.groupTab,0,static_cast<int>(members.size())-1);Save();}
void Controller::Remove(std::wstring const& key){auto it=std::find_if(layout.zones.begin(),layout.zones.end(),[&](auto const& z){return z.id==key;});if(it==layout.zones.end())return;auto gid=it->group;PushUndo(i18n::TrF(L"删除分区「{0}」",{it->name}));layout.zones.erase(it);if(!gid.empty()&&GroupMemberIds(layout,gid).size()==1)for(auto& z:layout.zones)if(z.group==gid){z.group.clear();z.groupTab=0;}SyncWindows();Save();}
void Controller::Refresh(){for(auto& w:windows)w->Refresh();}
void Controller::Show(){for(auto& w:windows)w->Show();}
void Controller::HideAll(){for(auto& w:windows)if(IsWindowVisible(w->hwnd))ShowWindow(w->hwnd,SW_HIDE);}
void Controller::ToggleAll(){bool any=false;for(auto& w:windows)if(IsWindowVisible(w->hwnd)){any=true;break;}if(any)HideAll();else Show();}
void Controller::InstallCtrlHook(){if(ctrlHook||!messageWindow)return;g_ctrlTarget=messageWindow;g_lastCtrlDown=0;g_inCtrl=false;g_ctrlUpSeen=false;g_otherKey=false;ctrlHook=SetWindowsHookExW(WH_KEYBOARD_LL,CtrlHookProc,GetModuleHandleW(nullptr),0);}
void Controller::RemoveCtrlHook(){if(ctrlHook){UnhookWindowsHookEx(ctrlHook);ctrlHook=nullptr;}g_ctrlTarget=nullptr;}
void Controller::InstallRevealHook(){if(revealHook||!messageWindow)return;g_revealTarget=messageWindow;revealHook=SetWindowsHookExW(WH_KEYBOARD_LL,RevealHookProc,GetModuleHandleW(nullptr),0);}
void Controller::RemoveRevealHook(){if(revealHook){UnhookWindowsHookEx(revealHook);revealHook=nullptr;}g_revealTarget=nullptr;}
void Controller::RevealBegin(){if(revealShowing)return;revealSet.clear();for(auto& w:windows)if(IsWindow(w->hwnd)&&IsWindowVisible(w->hwnd))revealSet.push_back(w->hwnd);if(revealSet.empty())return;revealShowing=true;HideAll();}
void Controller::RevealEnd(){if(!revealShowing)return;revealShowing=false;for(auto& w:windows)if(std::find(revealSet.begin(),revealSet.end(),w->hwnd)!=revealSet.end())w->Show();revealSet.clear();if(!raising&&g_raiseKey.vk&&(GetAsyncKeyState(g_raiseKey.vk)&0x8000)&&RevealModsDown(g_raiseKey.mods))StartRaise();}
void Controller::StartRaise(){if(raising||revealShowing)return;int n=0;for(auto& w:windows)if(IsWindow(w->hwnd)&&IsWindowVisible(w->hwnd)){w->Raise(true);++n;}raising=n>0;}
void Controller::EndRaise(){if(!raising)return;raising=false;for(auto& w:windows)if(IsWindow(w->hwnd))w->Raise(false);}
bool Controller::ApplyHotkey(){if(messageWindow){UnregisterHotKey(messageWindow,1);UnregisterHotKey(messageWindow,2);UnregisterHotKey(messageWindow,3);UnregisterHotKey(messageWindow,4);}Hotkey hk;auto const& text=layout.settings.hotkey;bool doubleCtrl=IsDoubleCtrlHotkey(text);if(doubleCtrl)InstallCtrlHook();else RemoveCtrlHook();if(!text.empty()&&!doubleCtrl&&ParseHotkey(text,hk))RegisterHotKey(messageWindow,1,hk.mods|MOD_NOREPEAT,hk.vk);Hotkey hs;auto const& stext=layout.settings.hotkeySearch;bool ok=true;if(!stext.empty()&&ParseHotkey(stext,hs))ok=RegisterHotKey(messageWindow,2,hs.mods|MOD_NOREPEAT,hs.vk)!=0;Hotkey hc;auto const& ctext=layout.settings.hotkeyCapture;if(!ctext.empty()&&ParseHotkey(ctext,hc))ok=RegisterHotKey(messageWindow,3,hc.mods|MOD_NOREPEAT,hc.vk)!=0&&ok;Hotkey hu;auto const& utext=layout.settings.hotkeyUndo;if(!utext.empty()&&ParseHotkey(utext,hu))ok=RegisterHotKey(messageWindow,4,hu.mods|MOD_NOREPEAT,hu.vk)!=0&&ok;Hotkey rk,gr;bool wantReveal=!layout.settings.revealHotkey.empty()&&ParseHotkey(layout.settings.revealHotkey,rk);bool wantRaise=!layout.settings.hotkeyRaise.empty()&&ParseHotkey(layout.settings.hotkeyRaise,gr);bool keyChanged=rk.mods!=g_revealKey.mods||rk.vk!=g_revealKey.vk||gr.mods!=g_raiseKey.mods||gr.vk!=g_raiseKey.vk;g_revealKey=wantReveal?rk:Hotkey{};g_raiseKey=wantRaise?gr:Hotkey{};if(!wantReveal&&revealShowing)RevealEnd();if(raising&&!wantRaise)EndRaise();if(!wantReveal&&!wantRaise)RemoveRevealHook();else if(keyChanged||!revealHook){RemoveRevealHook();InstallRevealHook();}return ok;}
void Controller::Quit(){if(quitting)return;quitting=true;for(auto& w:windows)w->Capture();Save();Shell_NotifyIconW(NIM_DELETE,&tray);Application::Current().Exit();}
void Controller::ToggleDesktop(){if(raising)EndRaise();desktopMode=!desktopMode;host=shell::DesktopHost();for(auto& w:windows)w->SetDesktop(desktopMode);}
void Controller::ShowSettings(){if(!settings)settings=std::make_unique<SettingsWindow>(*this);settings->Apply();settings->Show();}
void Controller::CloseSettings(){settings.reset();}
void Controller::ShowTidy(){if(!tidy)tidy=std::make_unique<TidyWindow>(*this);tidy->Show();}
void Controller::CloseTidy(){tidy.reset();}
void Controller::ShowNote(){layout.widgets.noteVisible=true;if(!note)note=std::make_unique<NoteWindow>(*this);note->Show();Save();}
void Controller::CloseNote(){if(layout.widgets.noteVisible){layout.widgets.noteVisible=false;Save();}note.reset();}
void Controller::ShowTodo(){layout.widgets.todoVisible=true;if(!todo)todo=std::make_unique<TodoWindow>(*this);todo->Show();Save();}
void Controller::CloseTodo(){if(layout.widgets.todoVisible){layout.widgets.todoVisible=false;Save();}todo.reset();}
void Controller::ShowClock(){layout.widgets.clockVisible=true;if(!clockW)clockW=std::make_unique<ClockWindow>(*this);clockW->Show();Save();}
void Controller::CloseClock(){if(layout.widgets.clockVisible){layout.widgets.clockVisible=false;Save();}clockW.reset();}
void Controller::ShowGuide(){if(!guide)guide=std::make_unique<GuideWindow>(*this);guide->Show();}
void Controller::CloseGuide(){guide.reset();}
void Controller::ShowMusic(){layout.widgets.musicVisible=true;if(!music)music=std::make_unique<MusicWindow>(*this);music->Show();Save();}
void Controller::CloseMusic(){if(layout.widgets.musicVisible){layout.widgets.musicVisible=false;Save();}music.reset();}
void Controller::ShowSearch(){if(!search)search=std::make_unique<SearchWindow>(*this);search->Show();}
void Controller::CloseSearch(){search.reset();}
void Controller::ShowWeather(){layout.widgets.weatherVisible=true;if(!weather)weather=std::make_unique<WeatherWindow>(*this);weather->Show();Save();}
void Controller::CloseWeather(){if(layout.widgets.weatherVisible){layout.widgets.weatherVisible=false;Save();}weather.reset();}
void Controller::ShowAppGrid(){layout.widgets.appGridVisible=true;if(!appGrid)appGrid=std::make_unique<AppGridWindow>(*this);appGrid->Show();Save();}
void Controller::CloseAppGrid(){if(layout.widgets.appGridVisible){layout.widgets.appGridVisible=false;Save();}appGrid.reset();}
void Controller::ShowCapture(){if(!capture)capture=std::make_unique<CaptureWindow>(*this);else capture->Show();}
void Controller::CloseCapture(){capture.reset();}
void Controller::ShowPreview(std::vector<std::wstring> const& paths,size_t start){if(!preview)preview=std::make_unique<PreviewWindow>(*this);preview->Open(paths,start);}
void Controller::ClosePreview(){preview.reset();}
void Controller::RebuildWidgets(){
 note.reset();todo.reset();clockW.reset();music.reset();weather.reset();appGrid.reset();search.reset();
 for(auto& w:windows)w->ApplySettings();
 std::weak_ptr<bool> weak=syncAlive;
 winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread().TryEnqueue([this,weak]{
  auto alive=weak.lock();if(!alive||!*alive||quitting)return;
  auto& k=layout.widgets;
  if(k.noteVisible&&!note)note=std::make_unique<NoteWindow>(*this);
  if(k.todoVisible&&!todo)todo=std::make_unique<TodoWindow>(*this);
  if(k.clockVisible&&!clockW)clockW=std::make_unique<ClockWindow>(*this);
  if(k.musicVisible&&!music)music=std::make_unique<MusicWindow>(*this);
  if(k.weatherVisible&&!weather)weather=std::make_unique<WeatherWindow>(*this);
  if(k.appGridVisible&&!appGrid)appGrid=std::make_unique<AppGridWindow>(*this);
 });
}
bool Controller::CommitCapture(std::wstring const& text,bool asTodo){
 std::wstring preview=text;if(preview.size()>60)preview=preview.substr(0,60)+L"…";
 if(asTodo){if(!AddTodo(layout.widgets,text))return false;if(todo)todo->RefreshList();Save();}
 else{if(!AppendNote(layout.widgets,text))return false;if(note)note->Reload();Save();}
 NOTIFYICONDATAW nif{sizeof(nif)};nif.hWnd=messageWindow;nif.uID=1;nif.uFlags=NIF_INFO;nif.dwInfoFlags=NIIF_INFO;wcscpy_s(nif.szInfoTitle,i18n::Tr(L"快速捕获").c_str());wcscpy_s(nif.szInfo,i18n::TrF(asTodo?L"已添加待办「{0}」":L"已记入便签「{0}」",{preview}).c_str());Shell_NotifyIconW(NIM_MODIFY,&nif);
 return true;}
void Controller::Toast(std::wstring const& title,std::wstring const& text){NOTIFYICONDATAW nif{sizeof(nif)};nif.hWnd=messageWindow;nif.uID=1;nif.uFlags=NIF_INFO;nif.dwInfoFlags=NIIF_INFO;wcscpy_s(nif.szInfoTitle,title.c_str());wcscpy_s(nif.szInfo,text.c_str());Shell_NotifyIconW(NIM_MODIFY,&nif);}
void Controller::CheckReminders(){bool save=false;for(auto& t:layout.widgets.todos){if(t.done||t.reminded||!DueReached(t.due))continue;t.reminded=true;save=true;std::wstring text=t.text;if(text.size()>100)text=text.substr(0,100)+L"…";NOTIFYICONDATAW nif{sizeof(nif)};nif.hWnd=messageWindow;nif.uID=1;nif.uFlags=NIF_INFO;nif.dwInfoFlags=NIIF_INFO;wcscpy_s(nif.szInfoTitle,i18n::Tr(L"待办到期提醒").c_str());wcscpy_s(nif.szInfo,i18n::TrF(L"「{0}」已到截止日期",{text}).c_str());Shell_NotifyIconW(NIM_MODIFY,&nif);}if(save)Save();}
void Controller::ApplySettings(){for(auto& w:windows)w->ApplySettings();}
void Controller::MoveEntry(std::wstring const& key,std::wstring const& target,size_t index){auto to=std::find_if(layout.zones.begin(),layout.zones.end(),[&](auto const& z){return z.id==target;});if(to==layout.zones.end())return;for(auto& from:layout.zones){auto entry=std::find_if(from.entries.begin(),from.entries.end(),[&](auto const& e){return e.id==key;});if(entry==from.entries.end())continue;if(&from!=&*to){for(auto const& existing:to->entries)if(PathKey(existing.path)==PathKey(entry->path))return;PushUndo(i18n::TrF(L"移动入口到「{0}」",{to->name}));}auto value=*entry;if(&from!=&*to)value.stack.clear();auto old=static_cast<size_t>(entry-from.entries.begin());from.entries.erase(entry);if(&from==&*to && index>old)--index;index=std::min(index,to->entries.size());to->entries.insert(to->entries.begin()+index,std::move(value));return;}}
static void TrimMemoryIfIdle(){
 LASTINPUTINFO lii{sizeof(LASTINPUTINFO)};GetLastInputInfo(&lii);
 if(GetTickCount()-lii.dwTime<5*60*1000)return;
 static DWORD lastTrim=0;if(GetTickCount()-lastTrim<60*1000)return;
 PROCESS_MEMORY_COUNTERS pmc{};pmc.cb=sizeof(pmc);
 if(!GetProcessMemoryInfo(GetCurrentProcess(),&pmc,sizeof(pmc))||pmc.WorkingSetSize<200ull*1024*1024)return;
 SetProcessWorkingSetSize(GetCurrentProcess(),(SIZE_T)-1,(SIZE_T)-1);lastTrim=GetTickCount();
}
LRESULT CALLBACK Controller::MessageProc(HWND h,UINT msg,WPARAM w,LPARAM l){auto* self=reinterpret_cast<Controller*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(msg==WM_NCCREATE){self=static_cast<Controller*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}if(!self)return DefWindowProcW(h,msg,w,l);try{if(msg==self->taskbarCreated&&self->taskbarCreated){self->AddTray();if(self->desktopMode){if(self->raising)self->EndRaise();self->host=shell::DesktopHost();for(auto& item:self->windows)if(IsWindow(item->hwnd))item->SetDesktop(true);}}if(msg==WM_TIMER&&w==1){if(self->desktopMode){auto host=shell::DesktopHost();if(host&&host!=self->host){self->host=host;if(!self->raising)for(auto& item:self->windows)if(IsWindow(item->hwnd))item->SetDesktop(true);}}if(!self->raising)for(auto& item:self->windows)if(IsWindow(item->hwnd))item->EmbedRetry();
 if(self->raising&&!(g_raiseKey.vk&&(GetAsyncKeyState(g_raiseKey.vk)&0x8000)&&RevealModsDown(g_raiseKey.mods)))self->EndRaise();
 if(self->revealShowing&&!(g_revealKey.vk&&(GetAsyncKeyState(g_revealKey.vk)&0x8000)&&RevealModsDown(g_revealKey.mods)))self->RevealEnd();
 for(auto& z:self->layout.zones){if(z.mappedFolder.empty())continue;auto stamp=StampOf(z.mappedFolder);if(stamp.empty())continue;auto key=PathKey(z.mappedFolder);auto it=self->mappedStamp.find(key);if(it==self->mappedStamp.end()){self->mappedStamp[key]=stamp;continue;}if(it->second!=stamp){it->second=stamp;SyncMapped(z);for(auto& item:self->windows)if(item->id==z.id||item->viewId==z.id)item->Refresh();}}self->CheckReminders();static int healTick=0;if((++healTick%5)==0){self->ApplyHotkey();self->HealTopology();}if(self->layout.settings.memTrim)TrimMemoryIfIdle();}if(msg==WM_TIMER&&w==2){KillTimer(h,2);self->SyncUploadAuto();return 0;}if(msg==WM_APP+3){for(auto& item:self->windows)item->Notify(i18n::Tr(L"自动同步上传失败：请检查网络或 WebDAV 设置。"));return 0;}if(msg==WM_APP+2){self->Show();if(!self->windows.empty())SetForegroundWindow(self->windows.front()->hwnd);return 0;}if(msg==WM_HOTKEY&&w==1){self->ToggleAll();return 0;}if(msg==WM_HOTKEY&&w==2){self->ShowSearch();return 0;}if(msg==WM_HOTKEY&&w==3){self->ShowCapture();return 0;}if(msg==WM_HOTKEY&&w==4){self->Undo();return 0;}if(msg==WM_APP+4){self->ToggleAll();return 0;}if(msg==WM_APP+5){self->RevealBegin();return 0;}if(msg==WM_APP+6){self->RevealEnd();return 0;}if(msg==WM_APP+7){self->StartRaise();return 0;}if(msg==WM_APP+8){self->EndRaise();return 0;}if(msg==WM_APP+1){if(l==WM_LBUTTONUP)self->Show();if(l==WM_RBUTTONUP){HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,i18n::Tr(L"新增分区").c_str());
 {static wchar_t const* tags[]={L"downloads",L"documents",L"pictures",L"music",L"videos"};HMENU quick=CreatePopupMenu();for(int i=0;i<5;++i)AppendMenuW(quick,MF_STRING,21+i,KnownFolderName(tags[i]).c_str());AppendMenuW(menu,MF_POPUP,(UINT_PTR)quick,i18n::Tr(L"快速分区").c_str());
  auto tpls=BuiltInTemplates();HMENU tmenu=CreatePopupMenu();for(int i=0;i<static_cast<int>(tpls.size());++i)AppendMenuW(tmenu,MF_STRING,31+i,i18n::Tr(tpls[i].name).c_str());AppendMenuW(menu,MF_POPUP,(UINT_PTR)tmenu,i18n::Tr(L"分区模板").c_str());}
AppendMenuW(menu,MF_STRING,2,i18n::Tr(L"显示全部").c_str());AppendMenuW(menu,MF_STRING,9,i18n::Tr(L"全部隐藏").c_str());AppendMenuW(menu,MF_STRING,3,self->desktopMode?i18n::Tr(L"切换普通窗口").c_str():i18n::Tr(L"试验桌面嵌入").c_str());AppendMenuW(menu,MF_STRING,6,i18n::Tr(L"整理桌面…").c_str());AppendMenuW(menu,MF_STRING|(self->layout.widgets.noteVisible?MF_CHECKED:0),7,i18n::Tr(L"便签").c_str());AppendMenuW(menu,MF_STRING|(self->layout.widgets.todoVisible?MF_CHECKED:0),8,i18n::Tr(L"待办").c_str());AppendMenuW(menu,MF_STRING|(self->layout.widgets.clockVisible?MF_CHECKED:0),10,i18n::Tr(L"时钟").c_str());AppendMenuW(menu,MF_STRING|(self->layout.widgets.musicVisible?MF_CHECKED:0),11,i18n::Tr(L"音乐").c_str());AppendMenuW(menu,MF_STRING,12,i18n::Tr(L"搜索").c_str());AppendMenuW(menu,MF_STRING|(self->layout.widgets.weatherVisible?MF_CHECKED:0),13,i18n::Tr(L"天气").c_str());AppendMenuW(menu,MF_STRING|(self->layout.widgets.appGridVisible?MF_CHECKED:0),16,i18n::Tr(L"应用网格").c_str());AppendMenuW(menu,MF_STRING,14,i18n::Tr(L"快速捕获…").c_str());
 {std::wstring label=self->undo.Empty()?i18n::Tr(L"撤销上一步"):i18n::TrF(L"撤销：{0}",{self->undo.TopLabel()});AppendMenuW(menu,MF_STRING|(self->undo.Empty()?MF_GRAYED:0),15,label.c_str());}
AppendMenuW(menu,MF_STRING,5,i18n::Tr(L"设置").c_str());AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,4,i18n::Tr(L"退出").c_str());POINT p{};GetCursorPos(&p);SetForegroundWindow(h);int cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,p.x,p.y,0,h,nullptr);DestroyMenu(menu);if(cmd==1)self->Add();
 if(cmd>=21&&cmd<=25){static wchar_t const* tags[]={L"downloads",L"documents",L"pictures",L"music",L"videos"};self->QuickZone(tags[cmd-21]);}
 if(cmd>=31&&cmd<=33){auto tpls=BuiltInTemplates();if(cmd-31<static_cast<int>(tpls.size()))self->UseTemplate(tpls[cmd-31]);}
 if(cmd==2)self->Show();if(cmd==9)self->HideAll();if(cmd==3)self->ToggleDesktop();if(cmd==6)self->ShowTidy();if(cmd==7){self->layout.widgets.noteVisible?self->CloseNote():self->ShowNote();}if(cmd==8){self->layout.widgets.todoVisible?self->CloseTodo():self->ShowTodo();}if(cmd==10){self->layout.widgets.clockVisible?self->CloseClock():self->ShowClock();}if(cmd==11){self->layout.widgets.musicVisible?self->CloseMusic():self->ShowMusic();}if(cmd==12)self->ShowSearch();if(cmd==13){self->layout.widgets.weatherVisible?self->CloseWeather():self->ShowWeather();}if(cmd==16){self->layout.widgets.appGridVisible?self->CloseAppGrid():self->ShowAppGrid();}if(cmd==14)self->ShowCapture();if(cmd==15)self->Undo();if(cmd==5)self->ShowSettings();if(cmd==4)self->Quit();}return 0;}}catch(...){MessageBoxW(h,i18n::Tr(L"操作未完成。").c_str(),L"GuoDesk",MB_OK|MB_ICONERROR);}return DefWindowProcW(h,msg,w,l);}
}


