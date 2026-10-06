#include "pch.h"
#include <shlwapi.h>
#include "ClockWindow.h"
#include "DeskWindow.h"
#include "Shell.h"
#include "I18n.h"
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
namespace Shapes=winrt::Microsoft::UI::Xaml::Shapes;
using namespace Microsoft::UI::Xaml::Media;
namespace guodesk {
static std::wstring ClockExePath(){wchar_t buf[MAX_PATH]{};GetModuleFileNameW(nullptr,buf,MAX_PATH);return buf;}
static std::vector<std::wstring> EnumFolderImages(std::wstring folder){
 while(!folder.empty()&&folder.back()==L'\\')folder.pop_back();
 std::vector<std::wstring> names;WIN32_FIND_DATAW fd{};auto h=FindFirstFileW((folder+L"\\*").c_str(),&fd);
 if(h!=INVALID_HANDLE_VALUE){do{if(!(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))names.push_back(folder+L"\\"+std::wstring(fd.cFileName));}while(FindNextFileW(h,&fd));FindClose(h);}
 auto imgs=FilterImagePaths(names);std::sort(imgs.begin(),imgs.end());return imgs;
}
static MenuFlyoutItem MenuItem(std::wstring const& text,std::function<void()> run){MenuFlyoutItem b;b.Text(text);b.Click([run](auto&&,auto&&){try{run();}catch(...){MessageBoxW(nullptr,i18n::Tr(L"操作未完成，请检查文件是否存在及访问权限。").c_str(),L"GuoDesk",MB_OK|MB_ICONERROR);}});return b;}
static Brush ResolveClockBrush(wchar_t const* key,Windows::UI::Color fallback,bool dark){
 try{return Application::Current().Resources().Lookup(box_value(key)).as<Brush>();}catch(...){}
 struct Entry{wchar_t const* key;Windows::UI::Color light;Windows::UI::Color dark;};
 static const Entry palette[]{
  {L"TextFillColorSecondary",{255,97,97,97},{255,199,199,199}},
 };
 for(auto const& e:palette)if(!wcscmp(e.key,key))return SolidColorBrush(dark?e.dark:e.light);
 return SolidColorBrush(fallback);
}
Brush ClockWindow::ThemeBrush(wchar_t const* key,Windows::UI::Color fallback){bool dark=false;try{dark=root.ActualTheme()==ElementTheme::Dark;}catch(...){}return ResolveClockBrush(key,fallback,dark);}
static Brush ClockInk(Grid const& root){bool dark=false;try{dark=root.ActualTheme()==ElementTheme::Dark;}catch(...){}return SolidColorBrush(dark?Windows::UI::Color{255,225,225,225}:Windows::UI::Color{255,60,60,60});}
static Brush ClockFaint(Grid const& root){bool dark=false;try{dark=root.ActualTheme()==ElementTheme::Dark;}catch(...){}return SolidColorBrush(dark?Windows::UI::Color{255,150,150,150}:Windows::UI::Color{255,130,130,130});}
static Brush ClockAccent(){try{return Application::Current().Resources().Lookup(box_value(L"AccentFillColorDefaultBrush")).as<Brush>();}catch(...){}return SolidColorBrush(Windows::UI::Color{255,0,120,212});}
void ClockWindow::BuildFace(){
 if(!analog||!face)return;
 double w=root.ActualWidth(),h=root.ActualHeight();if(w<40||h<40)return;
 double d=std::min(w,h-40);if(d<40)d=40;double r=d/2,cx=w/2,cy=h/2-2;
 face.Children().Clear();
 auto ink=ClockInk(root),faint=ClockFaint(root),accent=ClockAccent();
 Shapes::Ellipse ring;ring.Width(d);ring.Height(d);ring.Stroke(faint);ring.StrokeThickness(2);ring.Fill(SolidColorBrush(Windows::UI::Colors::Transparent()));Canvas::SetLeft(ring,cx-r);Canvas::SetTop(ring,cy-r);face.Children().Append(ring);
 for(int i=0;i<12;++i){double ang=i*3.14159265358979/6;bool major=i%3==0;double len=major?10:5;Shapes::Line t;t.X1(cx+sin(ang)*(r-2));t.Y1(cy-cos(ang)*(r-2));t.X2(cx+sin(ang)*(r-2-len));t.Y2(cy-cos(ang)*(r-2-len));t.Stroke(major?ink:faint);t.StrokeThickness(major?2:1);face.Children().Append(t);}
 auto makeHand=[&](double len,double thick,Brush brush){Shapes::Rectangle rect;rect.Width(thick);rect.Height(len);rect.Fill(brush);RotateTransform rt;rt.CenterX(thick/2);rt.CenterY(len);rect.RenderTransform(rt);Canvas::SetLeft(rect,cx-thick/2);Canvas::SetTop(rect,cy-len);return rect;};
 hourHand=makeHand(r*0.5,4,ink);minuteHand=makeHand(r*0.72,3,ink);secondHand=makeHand(r*0.78,1.5,accent);
 face.Children().Append(hourHand);face.Children().Append(minuteHand);face.Children().Append(secondHand);
 Shapes::Ellipse pin;pin.Width(7);pin.Height(7);pin.Fill(accent);Canvas::SetLeft(pin,cx-3.5);Canvas::SetTop(pin,cy-3.5);face.Children().Append(pin);
 Update();
}
void ClockWindow::Rotate(Shapes::Rectangle const& hand,double angle){if(!hand)return;RotateTransform rt;rt.CenterX(hand.Width()/2);rt.CenterY(hand.Height());rt.Angle(angle);hand.RenderTransform(rt);}
void ClockWindow::Update(){
 SYSTEMTIME st{};GetLocalTime(&st);
 if(analog){Rotate(hourHand,(st.wHour%12)*30.0+st.wMinute*0.5);Rotate(minuteHand,st.wMinute*6.0+st.wSecond*0.1);Rotate(secondHand,st.wSecond*6.0);}
 else{wchar_t buf[16]{};swprintf_s(buf,16,L"%02d:%02d",st.wHour,st.wMinute);time.Text(buf);}
 static wchar_t const* weekdays[]{L"星期日",L"星期一",L"星期二",L"星期三",L"星期四",L"星期五",L"星期六"};
 date.Text(i18n::TrF(L"{0}年{1}月{2}日 · {3}",{std::to_wstring(st.wYear),std::to_wstring(st.wMonth),std::to_wstring(st.wDay),i18n::Tr(weekdays[st.wDayOfWeek%7])}));
 auto ln=LunarFromSolar(st.wYear,st.wMonth,st.wDay);
 auto text=LunarText(ln);
 if(ln.valid){auto hol=HolidayText(st.wYear,st.wMonth,st.wDay,ln);if(!hol.empty())text+=L" · "+hol;}
 lunar.Text(text);
 lunar.Visibility(text.empty()?Visibility::Collapsed:Visibility::Visible);
}
void ClockWindow::SetBgBrush(std::wstring const& path){
 Brush next{nullptr};
 if(!path.empty()&&IsImagePath(path)&&GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES){
  try{wchar_t url[1024];DWORD c=1024;if(SUCCEEDED(UrlCreateFromPathW(path.c_str(),url,&c,0))){winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage img;img.UriSource(winrt::Windows::Foundation::Uri(url));ImageBrush ib;ib.ImageSource(img);ib.Stretch(Stretch::UniformToFill);ib.Opacity(static_cast<double>(owner.layout.widgets.clockBgTrans)/100.0);next=ib;}}catch(...){}
 }
 root.Background(next);
}
void ClockWindow::ApplyBackground(){
 auto const p=PerfMaterial(owner.layout.settings.perfTier)?owner.layout.widgets.clockBg:std::wstring();
 bgList.clear();
 if(!p.empty()){DWORD a=GetFileAttributesW(p.c_str());if(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY))bgList=EnumFolderImages(p);}
 if(bgList.empty())SetBgBrush(p);
 else{if(bgIdx>=bgList.size())bgIdx=0;SetBgBrush(bgList[bgIdx]);}
 if(bgList.size()>1){if(!rotate){rotate=root.DispatcherQueue().CreateTimer();rotate.Interval(std::chrono::seconds(60));rotate.Tick([this](auto&&,auto&&){if(closing||!IsWindow(hwnd)||bgList.empty())return;bgIdx=(bgIdx+1)%bgList.size();SetBgBrush(bgList[bgIdx]);});}rotate.Start();}
 else if(rotate)rotate.Stop();
}
void ClockWindow::PopulateMenu(MenuFlyout const& menu){
 auto& w=owner.layout.widgets;
 bool isDir=false;if(!w.clockBg.empty()){DWORD a=GetFileAttributesW(w.clockBg.c_str());isDir=a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY);}
 if(calFlyout)menu.Items().Append(MenuItem(i18n::Tr(L"日历"),[this]{try{calFlyout.ShowAt(root);}catch(...){}}));
 menu.Items().Append(MenuItem(w.clockBg.empty()||isDir?i18n::Tr(L"设置背景图…"):i18n::Tr(L"更换背景图…"),[this]{auto picked=shell::Pick(hwnd,false);if(picked.empty())return;if(!IsImagePath(picked.front())){MessageBoxW(hwnd,i18n::Tr(L"请选择图片文件（png / jpg / bmp / gif / webp / tif）").c_str(),L"GuoDesk",MB_OK|MB_ICONINFORMATION);return;}bgIdx=0;owner.layout.widgets.clockBg=picked.front();ApplyBackground();owner.Save();}));
 menu.Items().Append(MenuItem(isDir?i18n::Tr(L"更换轮播文件夹…"):i18n::Tr(L"文件夹轮播…"),[this]{auto picked=shell::Pick(hwnd,true);if(picked.empty())return;auto imgs=EnumFolderImages(picked.front());if(imgs.empty()){MessageBoxW(hwnd,i18n::Tr(L"该文件夹中没有图片文件。").c_str(),L"GuoDesk",MB_OK|MB_ICONINFORMATION);return;}bgIdx=0;owner.layout.widgets.clockBg=picked.front();ApplyBackground();owner.Save();}));
 if(!w.clockBg.empty()){
  menu.Items().Append(MenuItem(i18n::TrF(L"背景透明度：{0}%（点击切换）",{std::to_wstring(w.clockBgTrans)}),[this]{auto& q=owner.layout.widgets;q.clockBgTrans=NextOpacityStep(q.clockBgTrans);ApplyBackground();owner.Save();}));
  menu.Items().Append(MenuItem(i18n::Tr(L"清除背景图"),[this]{auto& q=owner.layout.widgets;q.clockBg.clear();bgIdx=0;ApplyBackground();owner.Save();}));
 }
}
ClockWindow::ClockWindow(Controller& c):owner(c){
 window=Window();window.Title(i18n::Tr(L"GuoDesk 时钟"));hwnd=shell::Handle(window);
 try{auto dir=std::filesystem::path(ClockExePath()).parent_path();window.AppWindow().SetIcon((dir/L"guodesk.ico").wstring());}catch(...){}
 window.SystemBackdrop(MakeBackdrop(owner.layout.settings.backdrop,owner.layout.settings.perfTier));
 try{auto presenter=window.AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();presenter.SetBorderAndTitleBar(true,false);window.AppWindow().IsShownInSwitchers(false);}catch(...){}
 auto& w=owner.layout.widgets;
 root=Grid();
 RowDefinition timeRow;timeRow.Height(GridLength{1,GridUnitType::Star});root.RowDefinitions().Append(timeRow);
 RowDefinition dateRow;dateRow.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(dateRow);
 RowDefinition lunarRow;lunarRow.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(lunarRow);
 analog=owner.layout.settings.clockStyle==L"analog";
 if(analog){face=Canvas();Grid::SetRow(face,0);root.Children().Append(face);}
 else{time=TextBlock();time.FontSize(ScaledFont(owner.layout.settings.textSize,42));time.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());time.HorizontalAlignment(HorizontalAlignment::Center);time.VerticalAlignment(VerticalAlignment::Center);Grid::SetRow(time,0);root.Children().Append(time);}
 date=TextBlock();date.FontSize(ScaledFont(owner.layout.settings.textSize,12));date.Margin(Thickness{0,0,0,10});date.HorizontalAlignment(HorizontalAlignment::Center);date.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));
 Grid::SetRow(date,1);root.Children().Append(date);
 lunar=TextBlock();lunar.FontSize(ScaledFont(owner.layout.settings.textSize,11));lunar.Margin(Thickness{0,0,0,10});lunar.HorizontalAlignment(HorizontalAlignment::Center);lunar.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,150,150,150}));
 Grid::SetRow(lunar,2);root.Children().Append(lunar);
 auto dragging=std::make_shared<bool>(false);auto dragStart=std::make_shared<POINT>();auto dragOrigin=std::make_shared<RECT>();
 auto dragTimer=root.DispatcherQueue().CreateTimer();dragTimer.Interval(std::chrono::milliseconds(16));
 auto EndDrag=[this,dragging,dragTimer](){if(!*dragging)return;*dragging=false;dragTimer.Stop();RECT r{};GetWindowRect(hwnd,&r);auto& w=owner.layout.widgets;w.clockX=r.left;w.clockY=r.top;MONITORINFOEXW mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);w.clockMon=mi.szDevice;w.clockMX=r.left-mi.rcWork.left;w.clockMY=r.top-mi.rcWork.top;owner.Save();};
 dragTimer.Tick([this,dragging,dragStart,dragOrigin,EndDrag](auto&&,auto&&){if(!(GetAsyncKeyState(VK_LBUTTON)&0x8000)){EndDrag();return;}POINT p{};GetCursorPos(&p);SetWindowPos(hwnd,nullptr,dragOrigin->left+p.x-dragStart->x,dragOrigin->top+p.y-dragStart->y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);});
 root.PointerPressed([this,dragging,dragStart,dragOrigin,dragTimer](auto&&,Input::PointerRoutedEventArgs const& a){*dragging=true;GetCursorPos(&*dragStart);GetWindowRect(hwnd,&*dragOrigin);root.CapturePointer(a.Pointer());a.Handled(true);dragTimer.Start();});
 root.PointerReleased([EndDrag](auto&&,auto&&){EndDrag();});
 root.PointerCaptureLost([EndDrag](auto&&,auto&&){EndDrag();});
 try{CalendarView cal;cal.MinWidth(300);cal.MinHeight(320);Flyout f;f.Content(cal);calFlyout=f;}catch(...){}
 MenuFlyout menu;menu.Opening([this,menu](auto&&,auto&&){menu.Items().Clear();PopulateMenu(menu);});root.ContextFlyout(menu);
 window.Content(root);
 if(analog)root.SizeChanged([this](auto&&,auto&&){BuildFace();});
 window.Closed([this](auto&&,auto&&){if(closing)return;closing=true;window.DispatcherQueue().TryEnqueue([this]{owner.CloseClock();});});
 int x=w.clockX,y=w.clockY,wd=w.clockW,ht=w.clockH;SetWindowPos(hwnd,nullptr,x,y,wd,ht,SWP_NOZORDER);
 MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
 if(y>mi.rcWork.bottom-40||y<mi.rcWork.top-20||x>mi.rcWork.right-60||x<mi.rcWork.left-40){x=std::clamp(x,(int)mi.rcWork.left,(int)mi.rcWork.right-100);y=std::clamp(y,(int)mi.rcWork.top,(int)mi.rcWork.bottom-60);SetWindowPos(hwnd,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}
 Update();
 ApplyBackground();
 tick=root.DispatcherQueue().CreateTimer();tick.Interval(std::chrono::seconds(1));tick.Tick([this](auto&&,auto&&){Update();});tick.Start();
 window.Activate();
}
void ClockWindow::Show(){Update();window.Activate();}
ClockWindow::~ClockWindow(){closing=true;if(tick)tick.Stop();if(rotate)rotate.Stop();try{window.Closed(nullptr);}catch(...){}if(IsWindow(hwnd))window.Close();}
}
