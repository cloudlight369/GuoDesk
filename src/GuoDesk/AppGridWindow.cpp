#include "pch.h"
#include "AppGridWindow.h"
#include "DeskWindow.h"
#include "Shell.h"
#include "I18n.h"
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
namespace guodesk {
static Brush ResolveAppGridBrush(wchar_t const* key,Windows::UI::Color fallback,bool dark){
 try{return Application::Current().Resources().Lookup(box_value(key)).as<Brush>();}catch(...){}
 struct Entry{wchar_t const* key;Windows::UI::Color light;Windows::UI::Color dark;};
 static const Entry palette[]{
  {L"CardBackgroundFillColorDefault",{255,248,248,248},{255,43,43,43}},
  {L"SubtleFillColorSecondaryBrush",{255,236,236,236},{255,56,56,56}},
  {L"TextFillColorSecondary",{255,97,97,97},{255,199,199,199}},
 };
 for(auto const& e:palette)if(!wcscmp(e.key,key))return SolidColorBrush(dark?e.dark:e.light);
 return SolidColorBrush(fallback);
}
Brush AppGridWindow::ThemeBrush(wchar_t const* key,Windows::UI::Color fallback){bool dark=false;try{dark=root.ActualTheme()==ElementTheme::Dark;}catch(...){}return ResolveAppGridBrush(key,fallback,dark);}
void AppGridWindow::Scan(){try{apps=shell::EnumerateApps();}catch(...){apps.clear();}}
void AppGridWindow::SaveGeometry(){
 if(!IsWindow(hwnd))return;
 RECT r{};GetWindowRect(hwnd,&r);
 auto& w=owner.layout.widgets;
 if(w.appGridX==r.left&&w.appGridY==r.top&&w.appGridW==r.right-r.left&&w.appGridH==r.bottom-r.top)return;
 w.appGridX=r.left;w.appGridY=r.top;w.appGridW=r.right-r.left;w.appGridH=r.bottom-r.top;
 MONITORINFOEXW mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
 w.appGridMon=mi.szDevice;w.appGridMX=r.left-mi.rcWork.left;w.appGridMY=r.top-mi.rcWork.top;
 owner.Save();
}
void AppGridWindow::Launch(AppShortcut const& a,std::wstring const& file){
 std::wstring params;
 if(!file.empty()){params=L'"'+file+L'"';if(!a.arguments.empty())params=a.arguments+L" "+params;}
 else params=a.arguments;
 SHELLEXECUTEINFOW e{sizeof(e)};e.lpFile=a.target.c_str();if(!params.empty())e.lpParameters=params.c_str();e.nShow=SW_SHOWNORMAL;e.fMask=SEE_MASK_FLAG_NO_UI;
 if(!ShellExecuteExW(&e))owner.Toast(i18n::Tr(L"应用网格"),i18n::TrF(L"无法启动「{0}」。",{a.name}));
}
fire_and_forget AppGridWindow::DropOn(AppShortcut a,DragEventArgs args){
 auto deferral=args.GetDeferral();
 auto weak=winrt::make_weak(root);
 try{
  auto data=args.DataView();
  if(data.Contains(StandardDataFormats::StorageItems())){
   auto items=co_await data.GetStorageItemsAsync();
   int n=0;
   for(auto const& item:items){if(n>=8)break;std::wstring p(item.Path());if(p.empty())continue;++n;Launch(a,p);}
  }
 }catch(...){}
 if(auto live=weak.get())live.DispatcherQueue().TryEnqueue([this]{SaveGeometry();});
 deferral.Complete();
}
void AppGridWindow::Rebuild(){
 list.Children().Clear();
 std::vector<AppShortcut> shown;
 for(auto const& a:apps)if(AppMatches(a.name,query))shown.push_back(a);
 if(count)count.Text(i18n::TrF(L"{0} 个应用",{std::to_wstring(shown.size())}));
 if(shown.empty()){
  TextBlock empty;empty.Text(apps.empty()?i18n::Tr(L"未找到开始菜单应用"):i18n::Tr(L"没有匹配的应用"));empty.FontSize(ScaledFont(owner.layout.settings.textSize,12));empty.HorizontalAlignment(HorizontalAlignment::Center);empty.Margin(Thickness{0,24,0,0});empty.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));list.Children().Append(empty);
  return;
 }
 double hostW=root.ActualWidth()>60?root.ActualWidth():static_cast<double>(owner.layout.widgets.appGridW);
 int cols=static_cast<int>(std::clamp(hostW/96.0,3.0,8.0));
 int tileW=static_cast<int>((hostW-16-(cols-1)*6)/cols);if(tileW<72)tileW=72;
 StackPanel row{nullptr};
 for(size_t i=0;i<shown.size();++i){
  if(i%static_cast<size_t>(cols)==0){row=StackPanel();row.Orientation(Orientation::Horizontal);row.Spacing(6);row.HorizontalAlignment(HorizontalAlignment::Center);list.Children().Append(row);}
  auto const& a=shown[i];
  Button tile;tile.Width(tileW);tile.MinHeight(84);tile.Padding(Thickness{4,6,4,6});tile.Background(ThemeBrush(L"CardBackgroundFillColorDefault",Windows::UI::Color{255,60,60,60}));tile.BorderThickness(Thickness{0});
  StackPanel cell;cell.Spacing(4);cell.HorizontalAlignment(HorizontalAlignment::Center);
  Image icon;icon.Width(32);icon.Height(32);try{shell::LoadIcon(a.linkPath,icon);}catch(...){}
  cell.Children().Append(icon);
  TextBlock name;name.Text(a.name);name.FontSize(ScaledFont(owner.layout.settings.textSize,11));name.TextWrapping(TextWrapping::Wrap);name.TextTrimming(TextTrimming::CharacterEllipsis);name.MaxLines(2);name.HorizontalAlignment(HorizontalAlignment::Center);name.TextAlignment(TextAlignment::Center);
  cell.Children().Append(name);
  tile.Content(cell);
  ToolTipService::SetToolTip(tile,box_value(a.name+L"\n"+a.target));
  auto shortcut=a;
  tile.Click([this,shortcut](auto&&,auto&&){Launch(shortcut);});
  tile.AllowDrop(true);
  tile.DragOver([](auto&&,DragEventArgs const& e){e.AcceptedOperation(e.DataView().Contains(StandardDataFormats::StorageItems())?DataPackageOperation::Copy:DataPackageOperation::None);e.Handled(true);});
  tile.Drop([this,shortcut](auto&&,DragEventArgs const& e){e.Handled(true);DropOn(shortcut,e);});
  row.Children().Append(tile);
 }
}
LRESULT CALLBACK AppGridWindow::Subclass(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data){
 auto* self=reinterpret_cast<AppGridWindow*>(data);
 if(msg==WM_EXITSIZEMOVE&&self)self->SaveGeometry();
 return DefSubclassProc(h,msg,w,l);
}
AppGridWindow::AppGridWindow(Controller& c):owner(c){
 window=Window();window.Title(i18n::Tr(L"GuoDesk 应用网格"));hwnd=shell::Handle(window);
 try{wchar_t buf[MAX_PATH]{};GetModuleFileNameW(nullptr,buf,MAX_PATH);auto dir=std::filesystem::path(buf).parent_path();window.AppWindow().SetIcon((dir/L"guodesk.ico").wstring());}catch(...){}
 window.SystemBackdrop(MakeBackdrop(owner.layout.settings.backdrop));
 try{auto presenter=window.AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();presenter.SetBorderAndTitleBar(true,false);window.AppWindow().IsShownInSwitchers(false);}catch(...){}
 auto& w=owner.layout.widgets;
 root=Grid();
 RowDefinition head;head.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(head);
 RowDefinition find;find.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(find);
 RowDefinition bodyRow;bodyRow.Height(GridLength{1,GridUnitType::Star});root.RowDefinitions().Append(bodyRow);
 RowDefinition foot;foot.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(foot);
 Border header;header.Padding(Thickness{12,8,12,4});
 Grid headGrid;ColumnDefinition hc1;hc1.Width(GridLength{1,GridUnitType::Star});headGrid.ColumnDefinitions().Append(hc1);ColumnDefinition hc2;hc2.Width(GridLength{0,GridUnitType::Auto});headGrid.ColumnDefinitions().Append(hc2);
 StackPanel headLeft;headLeft.Orientation(Orientation::Horizontal);headLeft.Spacing(8);
 FontIcon grip;grip.FontFamily(FontFamily(L"Segoe Fluent Icons"));grip.Glyph(L"\uE7C2");grip.FontSize(ScaledFont(owner.layout.settings.textSize,12));grip.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));headLeft.Children().Append(grip);
 TextBlock title;title.Text(i18n::Tr(L"应用网格"));title.FontSize(ScaledFont(owner.layout.settings.textSize,12));title.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));headLeft.Children().Append(title);
 Grid::SetColumn(headLeft,0);headGrid.Children().Append(headLeft);
 count=TextBlock();count.FontSize(ScaledFont(owner.layout.settings.textSize,12));count.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));count.VerticalAlignment(VerticalAlignment::Center);count.Margin(Thickness{0,0,8,0});Grid::SetColumn(count,1);headGrid.Children().Append(count);
 Button refresh;refresh.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));refresh.BorderThickness(Thickness{0});refresh.Padding(Thickness{4});refresh.MinWidth(0);
 FontIcon reload;reload.FontFamily(FontFamily(L"Segoe Fluent Icons"));reload.Glyph(L"\uE72C");reload.FontSize(ScaledFont(owner.layout.settings.textSize,12));reload.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,160,160,160}));refresh.Content(reload);
 ToolTipService::SetToolTip(refresh,box_value(i18n::Tr(L"重新扫描应用")));
 Grid::SetColumn(refresh,2);headGrid.Children().Append(refresh);
 header.Child(headGrid);root.Children().Append(header);
 auto dragging=std::make_shared<bool>(false);auto dragStart=std::make_shared<POINT>();auto dragOrigin=std::make_shared<RECT>();
 auto dragTimer=root.DispatcherQueue().CreateTimer();dragTimer.Interval(std::chrono::milliseconds(16));
 auto EndDrag=[this,dragging,dragTimer](){if(!*dragging)return;*dragging=false;dragTimer.Stop();SaveGeometry();};
 dragTimer.Tick([this,dragging,dragStart,dragOrigin,EndDrag](auto&&,auto&&){if(!(GetAsyncKeyState(VK_LBUTTON)&0x8000)){EndDrag();return;}POINT p{};GetCursorPos(&p);SetWindowPos(hwnd,nullptr,dragOrigin->left+p.x-dragStart->x,dragOrigin->top+p.y-dragStart->y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);});
 header.PointerPressed([this,header,dragging,dragStart,dragOrigin,dragTimer](auto&&,Input::PointerRoutedEventArgs const& a){*dragging=true;GetCursorPos(&*dragStart);GetWindowRect(hwnd,&*dragOrigin);header.CapturePointer(a.Pointer());a.Handled(true);dragTimer.Start();});
 header.PointerReleased([EndDrag](auto&&,auto&&){EndDrag();});
 header.PointerCaptureLost([EndDrag](auto&&,auto&&){EndDrag();});
 auto searchBar=Border();searchBar.Padding(Thickness{10,2,10,4});
 input=TextBox();input.PlaceholderText(i18n::Tr(L"搜索应用"));input.FontSize(ScaledFont(owner.layout.settings.textSize,13));
 input.TextChanged([this](auto&&,auto&&){query=input.Text();query.erase(std::remove_if(query.begin(),query.end(),[](wchar_t ch){return ch==L'\r'||ch==L'\n';}),query.end());Rebuild();});
 refresh.Click([this](auto&&,auto&&){Scan();Rebuild();});
 searchBar.Child(input);Grid::SetRow(searchBar,1);root.Children().Append(searchBar);
 auto scroll=ScrollViewer();scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);scroll.Padding(Thickness{8,0,8,4});
 list=StackPanel();list.Spacing(6);scroll.Content(list);Grid::SetRow(scroll,2);root.Children().Append(scroll);
 TextBlock hint;hint.Text(i18n::Tr(L"点击启动 · 把文件拖到磁贴即用该应用打开"));hint.FontSize(ScaledFont(owner.layout.settings.textSize,11));hint.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));hint.HorizontalAlignment(HorizontalAlignment::Center);hint.Margin(Thickness{0,2,0,8});
 Grid::SetRow(hint,3);root.Children().Append(hint);
 root.SizeChanged([this](auto&&,auto&&){static double lastW=-1;double wpx=root.ActualWidth();if(wpx<=0)return;int cols=static_cast<int>(std::clamp(wpx/96.0,3.0,8.0));static int lastCols=-1;if(cols==lastCols&&wpx==lastW)return;lastCols=cols;lastW=wpx;Rebuild();});
 window.Content(root);
 window.Closed([this](auto&&,auto&&){if(closing)return;closing=true;window.DispatcherQueue().TryEnqueue([this]{owner.CloseAppGrid();});});
 int x=w.appGridX,y=w.appGridY,wd=w.appGridW,ht=w.appGridH;SetWindowPos(hwnd,nullptr,x,y,wd,ht,SWP_NOZORDER);
 MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
 if(y>mi.rcWork.bottom-40||y<mi.rcWork.top-20||x>mi.rcWork.right-60||x<mi.rcWork.left-40){x=std::clamp(x,(int)mi.rcWork.left,(int)mi.rcWork.right-100);y=std::clamp(y,(int)mi.rcWork.top,(int)mi.rcWork.bottom-60);SetWindowPos(hwnd,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}
 SetWindowSubclass(hwnd,Subclass,1,reinterpret_cast<DWORD_PTR>(this));
 Scan();Rebuild();
 window.Activate();
}
void AppGridWindow::Show(){Rebuild();window.Activate();}
AppGridWindow::~AppGridWindow(){closing=true;try{window.Closed(nullptr);}catch(...){}if(IsWindow(hwnd))window.Close();}
}
