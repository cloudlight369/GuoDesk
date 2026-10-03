#include "pch.h"
#include "WeatherWindow.h"
#include "DeskWindow.h"
#include "Shell.h"
#include "I18n.h"
#include <winhttp.h>
#include <thread>
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
namespace guodesk {
static std::wstring WeatherExePath(){wchar_t buf[MAX_PATH]{};GetModuleFileNameW(nullptr,buf,MAX_PATH);return buf;}
static Brush ResolveWeatherBrush(wchar_t const* key,Windows::UI::Color fallback,bool dark){
 try{return Application::Current().Resources().Lookup(box_value(key)).as<Brush>();}catch(...){}
 struct Entry{wchar_t const* key;Windows::UI::Color light;Windows::UI::Color dark;};
 static const Entry palette[]{{L"TextFillColorSecondary",{255,97,97,97},{255,199,199,199}}};
 for(auto const& e:palette)if(!wcscmp(e.key,key))return SolidColorBrush(dark?e.dark:e.light);
 return SolidColorBrush(fallback);
}
Brush WeatherWindow::ThemeBrush(wchar_t const* key,Windows::UI::Color fallback){bool dark=false;try{dark=root.ActualTheme()==ElementTheme::Dark;}catch(...){}return ResolveWeatherBrush(key,fallback,dark);}
std::wstring UrlParam(std::wstring const& value){
 std::string utf8=winrt::to_string(value);
 std::wstring out;
 for(unsigned char c:utf8){
  if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out+=static_cast<wchar_t>(c);
  else{wchar_t b[4]{};swprintf_s(b,4,L"%%%02X",c);out+=b;}
 }
 return out;
}
std::string HttpGetJson(std::wstring const& host,std::wstring const& path,bool secure){
 std::string out;
 HINTERNET ses=WinHttpOpen(L"GuoDesk/0.9.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
 if(!ses)return out;
 HINTERNET con=WinHttpConnect(ses,host.c_str(),secure?INTERNET_DEFAULT_HTTPS_PORT:INTERNET_DEFAULT_HTTP_PORT,0);
 if(con){
  HINTERNET req=WinHttpOpenRequest(con,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,secure?WINHTTP_FLAG_SECURE:0);
  if(req){
   if(WinHttpSendRequest(req,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)&&WinHttpReceiveResponse(req,nullptr)){
    DWORD status=0,cb=sizeof(status);
    if(WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&cb,WINHTTP_NO_HEADER_INDEX)&&status==200){
     char buf[8192];DWORD rd=0;
     while(WinHttpReadData(req,buf,sizeof(buf),&rd)&&rd)out.append(buf,rd);
    }
   }
   WinHttpCloseHandle(req);
  }
  WinHttpCloseHandle(con);
 }
 WinHttpCloseHandle(ses);
 return out;
}
static std::wstring DayLabel(long long date){
 SYSTEMTIME in{};in.wYear=static_cast<WORD>(date/10000);in.wMonth=static_cast<WORD>(date/100%100);in.wDay=static_cast<WORD>(date%100);
 if(date<=0||in.wMonth<1||in.wMonth>12||in.wDay<1||in.wDay>31)return L"";
 FILETIME f{};if(!SystemTimeToFileTime(&in,&f))return L"";
 SYSTEMTIME out{};FileTimeToSystemTime(&f,&out);
 static wchar_t const* weekdays[]{L"周日",L"周一",L"周二",L"周三",L"周四",L"周五",L"周六"};
 return weekdays[out.wDayOfWeek%7];
}
void WeatherWindow::SetStatus(std::wstring const& text){updated.Text(text);}
static Windows::UI::Color SkinColor(unsigned rgb){return Windows::UI::Color{255,static_cast<BYTE>((rgb>>16)&0xFF),static_cast<BYTE>((rgb>>8)&0xFF),static_cast<BYTE>(rgb&0xFF)};}
void WeatherWindow::ApplySkin(int code,bool night){
 Windows::UI::Color light{255,245,247,250},dim{255,220,226,234};
 if(owner.layout.widgets.weatherSkin!=1){
  skinMode=0;
  root.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));
  city.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));
  temp.Foreground(ThemeBrush(L"TextFillColorPrimary",Windows::UI::Color{255,0,0,0}));
  desc.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));
  extra.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));
  updated.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));
  return;
 }
 skinMode=1;
 int idx=WeatherSkinForCode(code,night);
 LinearGradientBrush g;g.StartPoint(Windows::Foundation::Point{0.f,0.f});g.EndPoint(Windows::Foundation::Point{0.f,1.f});
 GradientStop s1;s1.Color(SkinColor(WeatherSkinTop(idx)));s1.Offset(0.f);
 GradientStop s2;s2.Color(SkinColor(WeatherSkinBottom(idx)));s2.Offset(1.f);
 g.GradientStops().Append(s1);g.GradientStops().Append(s2);
 root.Background(g);
 city.Foreground(SolidColorBrush(dim));
 temp.Foreground(SolidColorBrush(light));
 desc.Foreground(SolidColorBrush(light));
 extra.Foreground(SolidColorBrush(dim));
 updated.Foreground(SolidColorBrush(dim));
}
void WeatherWindow::ResolveCoords(){
 auto& wg=owner.layout.widgets;
 if(wg.weatherLat>=-90&&wg.weatherLat<=90&&wg.weatherLon>=-180&&wg.weatherLon<=180){
  lat=wg.weatherLat;lon=wg.weatherLon;
  cityName=wg.weatherCity.empty()?i18n::Tr(L"自定义位置"):wg.weatherCity;
  resolved=true;
  Fetch();
  return;
 }
 auto weakAlive=std::weak_ptr<bool>(alive);auto dq=window.DispatcherQueue();
 cityName.clear();
 SetStatus(i18n::Tr(L"正在定位…"));
 std::thread([weakAlive,dq,this](){
  auto json=HttpGetJson(L"ip-api.com",L"/json/?lang=zh-CN&fields=status,city,lat,lon",false);
  auto place=ParseIpLocJson(std::wstring(winrt::to_hstring(json)));
  dq.TryEnqueue([weakAlive,dq,this,place]{
   if(!weakAlive.lock()||closing)return;
   if(place.name.empty()){SetStatus(i18n::Tr(L"自动定位失败，可在设置中搜索城市。"));return;}
   lat=place.lat;lon=place.lon;cityName=place.name;resolved=true;
   Fetch();
  });
 }).detach();
}
void WeatherWindow::Fetch(){
 if(!resolved){ResolveCoords();return;}
 auto weakAlive=std::weak_ptr<bool>(alive);auto dq=window.DispatcherQueue();
 double la=lat,lo=lon;
 SetStatus(i18n::Tr(L"正在更新…"));
 std::thread([weakAlive,dq,this,la,lo](){
  wchar_t path[512]{};
  swprintf_s(path,512,L"/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,relative_humidity_2m,weather_code,is_day&daily=weather_code,temperature_2m_max,temperature_2m_min&hourly=temperature_2m,weather_code&timezone=auto&forecast_days=7",la,lo);
  auto json=HttpGetJson(L"api.open-meteo.com",path,true);
  auto data=ParseWeatherJson(std::wstring(winrt::to_hstring(json)));
  dq.TryEnqueue([weakAlive,dq,this,data]{
   if(!weakAlive.lock()||closing)return;
   if(!data.valid){SetStatus(i18n::Tr(L"天气获取失败，点击右下角刷新重试。"));return;}
   Render(data);
   SYSTEMTIME st{};GetLocalTime(&st);
   wchar_t ts[20]{};swprintf_s(ts,20,L"%02d:%02d",st.wHour,st.wMinute);
   SetStatus(i18n::TrF(L"更新于 {0}",{ts}));
  });
 }).detach();
}
void WeatherWindow::Render(WeatherNow const& w){
 ApplySkin(w.code,!w.isDay);
 city.Text(cityName.empty()?i18n::Tr(L"天气"):cityName);
 icon.Text(WmoEmoji(w.code));
 wchar_t buf[24]{};swprintf_s(buf,24,L"%.0f°",w.temp);temp.Text(buf);
 desc.Text(i18n::Tr(WmoText(w.code)));
 wchar_t ebuf[80]{};
 if(w.humidity>0)swprintf_s(ebuf,80,L"%.0f° / %.0f° · %s %d%%",w.hi,w.lo,i18n::Tr(L"湿度").c_str(),w.humidity);
 else swprintf_s(ebuf,80,L"%.0f° / %.0f°",w.hi,w.lo);
 extra.Text(ebuf);
 hours.Children().Clear();
 Windows::UI::Color light{255,245,247,250};
 for(auto const& h:w.hours){
  auto col=StackPanel();col.Width(44);col.Spacing(2);
  TextBlock ht;wchar_t tb[8]{};swprintf_s(tb,8,L"%02lld",h.key%100);ht.Text(tb);ht.HorizontalAlignment(HorizontalAlignment::Center);ht.FontSize(ScaledFont(owner.layout.settings.textSize,11));ht.Foreground(SolidColorBrush(skinMode?light:Windows::UI::Color{255,120,120,120}));
  TextBlock hi;hi.Text(WmoEmoji(h.code));hi.FontSize(ScaledFont(owner.layout.settings.textSize,14));hi.HorizontalAlignment(HorizontalAlignment::Center);
  TextBlock ht2;wchar_t hb[12]{};swprintf_s(hb,12,L"%.0f°",h.temp);ht2.Text(hb);ht2.HorizontalAlignment(HorizontalAlignment::Center);ht2.FontSize(ScaledFont(owner.layout.settings.textSize,11));ht2.Foreground(SolidColorBrush(skinMode?light:Windows::UI::Color{255,120,120,120}));
  col.Children().Append(ht);col.Children().Append(hi);col.Children().Append(ht2);
  hours.Children().Append(col);
 }
 hours.Visibility(w.hours.empty()?Visibility::Collapsed:Visibility::Visible);
 days.Children().Clear();
 for(auto const& d:w.days){
  auto row=Grid();row.Margin(Thickness{0,4,0,4});
  ColumnDefinition c0;c0.Width(GridLength{0,GridUnitType::Auto});row.ColumnDefinitions().Append(c0);
  ColumnDefinition c1;c1.Width(GridLength{1,GridUnitType::Star});row.ColumnDefinitions().Append(c1);
  ColumnDefinition c2;c2.Width(GridLength{0,GridUnitType::Auto});row.ColumnDefinitions().Append(c2);
  TextBlock label;label.Text(DayLabel(d.date));label.FontSize(ScaledFont(owner.layout.settings.textSize,12));label.Foreground(SolidColorBrush(skinMode?light:Windows::UI::Color{255,120,120,120}));
  Grid::SetColumn(label,0);row.Children().Append(label);
  TextBlock ic;ic.Text(WmoEmoji(d.code));ic.FontSize(ScaledFont(owner.layout.settings.textSize,14));ic.HorizontalAlignment(HorizontalAlignment::Center);
  Grid::SetColumn(ic,1);row.Children().Append(ic);
  TextBlock range;wchar_t rb[32]{};swprintf_s(rb,32,L"%.0f° / %.0f°",d.lo,d.hi);range.Text(rb);range.FontSize(ScaledFont(owner.layout.settings.textSize,12));
  range.Foreground(SolidColorBrush(skinMode?light:Windows::UI::Color{255,120,120,120}));
  Grid::SetColumn(range,2);row.Children().Append(range);
  days.Children().Append(row);
 }
 days.Visibility(w.days.empty()?Visibility::Collapsed:Visibility::Visible);
}
void WeatherWindow::SaveGeometry(){
 if(!IsWindow(hwnd))return;
 RECT r{};GetWindowRect(hwnd,&r);
 auto& w=owner.layout.widgets;
 if(w.weatherX==r.left&&w.weatherY==r.top&&w.weatherW==r.right-r.left&&w.weatherH==r.bottom-r.top)return;
 w.weatherX=r.left;w.weatherY=r.top;w.weatherW=r.right-r.left;w.weatherH=r.bottom-r.top;
 MONITORINFOEXW mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
 w.weatherMon=mi.szDevice;w.weatherMX=r.left-mi.rcWork.left;w.weatherMY=r.top-mi.rcWork.top;
 owner.Save();
}
WeatherWindow::WeatherWindow(Controller& c):owner(c){
 window=Window();window.Title(i18n::Tr(L"GuoDesk 天气"));hwnd=shell::Handle(window);
 try{auto dir=std::filesystem::path(WeatherExePath()).parent_path();window.AppWindow().SetIcon((dir/L"guodesk.ico").wstring());}catch(...){}
 window.SystemBackdrop(MicaBackdrop());
 try{auto presenter=window.AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();presenter.SetBorderAndTitleBar(true,false);window.AppWindow().IsShownInSwitchers(false);}catch(...){}
 auto& w=owner.layout.widgets;
 root=Grid();
 GridLength rows[]={GridLength{0,GridUnitType::Auto},GridLength{0,GridUnitType::Auto},GridLength{0,GridUnitType::Auto},GridLength{0,GridUnitType::Auto},GridLength{0,GridUnitType::Auto},GridLength{1,GridUnitType::Star},GridLength{0,GridUnitType::Auto}};
 for(auto& r:rows){RowDefinition rd;rd.Height(r);root.RowDefinitions().Append(rd);}
 auto head=Border();head.Padding(Thickness{14,10,10,2});head.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));
 city=TextBlock();city.FontSize(ScaledFont(owner.layout.settings.textSize,13));city.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));
 head.Child(city);root.Children().Append(head);
 auto dragHeader=[&](FrameworkElement const& el){
  auto dragging=std::make_shared<bool>(false);auto dragStart=std::make_shared<POINT>();auto dragOrigin=std::make_shared<RECT>();
  dragTimer=root.DispatcherQueue().CreateTimer();dragTimer.Interval(std::chrono::milliseconds(16));
  dragTimer.Tick([this,dragging,dragStart,dragOrigin](auto&&,auto&&){
   if(!*dragging)return;
   if(!(GetAsyncKeyState(VK_LBUTTON)&0x8000)){*dragging=false;this->dragTimer.Stop();SaveGeometry();return;}
   POINT p{};GetCursorPos(&p);
   SetWindowPos(hwnd,nullptr,dragOrigin->left+p.x-dragStart->x,dragOrigin->top+p.y-dragStart->y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
  });
  el.PointerPressed([this,dragging,dragStart,dragOrigin,el](auto&&,Input::PointerRoutedEventArgs const& a){*dragging=true;GetCursorPos(&*dragStart);GetWindowRect(hwnd,&*dragOrigin);el.CapturePointer(a.Pointer());a.Handled(true);dragTimer.Start();});
  el.PointerReleased([this,dragging](auto&&,auto&&){if(*dragging){*dragging=false;dragTimer.Stop();SaveGeometry();}});
  el.PointerCaptureLost([this,dragging](auto&&,auto&&){if(*dragging){*dragging=false;dragTimer.Stop();SaveGeometry();}});
 };
 dragHeader(head);
 auto main=StackPanel();main.Orientation(Orientation::Horizontal);main.Spacing(14);main.Margin(Thickness{16,4,16,0});main.VerticalAlignment(VerticalAlignment::Top);
 icon=TextBlock();icon.FontSize(ScaledFont(owner.layout.settings.textSize,46));icon.VerticalAlignment(VerticalAlignment::Center);
 auto tcol=StackPanel();tcol.Spacing(0);
 temp=TextBlock();temp.FontSize(ScaledFont(owner.layout.settings.textSize,36));temp.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
 desc=TextBlock();desc.FontSize(ScaledFont(owner.layout.settings.textSize,13));desc.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));
 tcol.Children().Append(temp);tcol.Children().Append(desc);
 main.Children().Append(icon);main.Children().Append(tcol);
 Grid::SetRow(main,1);root.Children().Append(main);
 extra=TextBlock();extra.Margin(Thickness{16,6,16,0});extra.FontSize(ScaledFont(owner.layout.settings.textSize,12));extra.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));
 Grid::SetRow(extra,2);root.Children().Append(extra);
 auto divider=Border();divider.Height(1);divider.Margin(Thickness{16,10,16,2});divider.Background(SolidColorBrush(Windows::UI::Color{80,128,128,128}));
 Grid::SetRow(divider,3);root.Children().Append(divider);
 hours=StackPanel();hours.Orientation(Orientation::Horizontal);hours.Margin(Thickness{16,6,16,0});
 auto hoursHost=ScrollViewer();hoursHost.HorizontalScrollMode(ScrollMode::Enabled);hoursHost.VerticalScrollMode(ScrollMode::Disabled);hoursHost.VerticalScrollBarVisibility(ScrollBarVisibility::Disabled);hoursHost.HorizontalScrollBarVisibility(ScrollBarVisibility::Hidden);hoursHost.Height(64);hoursHost.Content(hours);
 Grid::SetRow(hoursHost,4);root.Children().Append(hoursHost);
 days=StackPanel();days.Margin(Thickness{16,4,16,0});
 auto daysHost=ScrollViewer();daysHost.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);daysHost.Content(days);
 Grid::SetRow(daysHost,5);root.Children().Append(daysHost);
 auto foot=Grid();foot.Margin(Thickness{16,2,12,10});
 ColumnDefinition f0;f0.Width(GridLength{1,GridUnitType::Star});foot.ColumnDefinitions().Append(f0);
 ColumnDefinition f1;f1.Width(GridLength{0,GridUnitType::Auto});foot.ColumnDefinitions().Append(f1);
 updated=TextBlock();updated.FontSize(ScaledFont(owner.layout.settings.textSize,11));updated.TextWrapping(TextWrapping::Wrap);updated.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));updated.VerticalAlignment(VerticalAlignment::Center);
 Grid::SetColumn(updated,0);foot.Children().Append(updated);
 auto refreshBtn=Button();refreshBtn.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));refreshBtn.BorderThickness(Thickness{0});refreshBtn.Padding(Thickness{6,2,6,2});refreshBtn.MinWidth(0);
 FontIcon ricon;ricon.FontFamily(FontFamily(L"Segoe Fluent Icons"));ricon.Glyph(L"\uE72C");ricon.FontSize(ScaledFont(owner.layout.settings.textSize,12));ricon.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));
 refreshBtn.Content(ricon);
 refreshBtn.Click([this](auto&&,auto&&){resolved=false;ResolveCoords();});
 Grid::SetColumn(refreshBtn,1);foot.Children().Append(refreshBtn);
 Grid::SetRow(foot,6);root.Children().Append(foot);
 window.Content(root);
 window.Closed([this](auto&&,auto&&){
  if(closing)return;closing=true;
  SaveGeometry();
  window.DispatcherQueue().TryEnqueue([this]{owner.CloseWeather();});
 });
 int x=w.weatherX,y=w.weatherY,wd=w.weatherW,ht=w.weatherH;SetWindowPos(hwnd,nullptr,x,y,wd,ht,SWP_NOZORDER);
 MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
 if(y>mi.rcWork.bottom-40||y<mi.rcWork.top-20||x>mi.rcWork.right-60||x<mi.rcWork.left-40){x=std::clamp(x,(int)mi.rcWork.left,(int)mi.rcWork.right-100);y=std::clamp(y,(int)mi.rcWork.top,(int)mi.rcWork.bottom-60);SetWindowPos(hwnd,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}
 temp.Text(L"--°");desc.Text(i18n::Tr(L"准备中…"));extra.Text(L"");
 ResolveCoords();
 refresh=root.DispatcherQueue().CreateTimer();refresh.Interval(std::chrono::minutes(30));refresh.Tick([this](auto&&,auto&&){Fetch();});refresh.Start();
 window.Activate();
}
void WeatherWindow::Show(){window.Activate();}
void WeatherWindow::Reload(){resolved=false;if(owner.layout.widgets.weatherSkin!=1)ApplySkin(0,false);ResolveCoords();}
WeatherWindow::~WeatherWindow(){
 closing=true;*alive=false;
 if(refresh)refresh.Stop();if(dragTimer)dragTimer.Stop();
 try{window.Closed(nullptr);}catch(...){}
 if(IsWindow(hwnd))window.Close();
}
}
