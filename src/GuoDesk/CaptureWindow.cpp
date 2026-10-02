#include "pch.h"
#include "CaptureWindow.h"
#include "DeskWindow.h"
#include "Shell.h"
#include "I18n.h"
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
namespace guodesk {
static std::wstring CaptureExePath(){wchar_t buf[MAX_PATH]{};GetModuleFileNameW(nullptr,buf,MAX_PATH);return buf;}
static Brush ResolveCaptureBrush(wchar_t const* key,Windows::UI::Color fallback,bool dark){
 try{return Application::Current().Resources().Lookup(box_value(key)).as<Brush>();}catch(...){}
 struct Entry{wchar_t const* key;Windows::UI::Color light;Windows::UI::Color dark;};
 static const Entry palette[]{
  {L"TextFillColorSecondary",{255,97,97,97},{255,199,199,199}},
  {L"CardBackgroundFillColorDefault",{255,248,248,248},{255,43,43,43}},
 };
 for(auto const& e:palette)if(!wcscmp(e.key,key))return SolidColorBrush(dark?e.dark:e.light);
 return SolidColorBrush(fallback);
}
Brush CaptureWindow::ThemeBrush(wchar_t const* key,Windows::UI::Color fallback){bool dark=false;try{dark=root.ActualTheme()==ElementTheme::Dark;}catch(...){}return ResolveCaptureBrush(key,fallback,dark);}
void CaptureWindow::Commit(bool asTodo){
 auto text=std::wstring(body.Text());
 body.Text(L"");
 if(owner.CommitCapture(text,asTodo))body.Focus(FocusState::Programmatic);
}
CaptureWindow::CaptureWindow(Controller& c):owner(c){
 window=Window();window.Title(i18n::Tr(L"GuoDesk 快速捕获"));hwnd=shell::Handle(window);
 try{auto dir=std::filesystem::path(CaptureExePath()).parent_path();window.AppWindow().SetIcon((dir/L"guodesk.ico").wstring());}catch(...){}
 window.SystemBackdrop(MicaBackdrop());
 try{auto presenter=window.AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();presenter.SetBorderAndTitleBar(true,false);window.AppWindow().IsShownInSwitchers(false);}catch(...){}
 root=Grid();root.Padding(Thickness{0,0,0,0});
 RowDefinition bodyRow;bodyRow.Height(GridLength{1,GridUnitType::Star});root.RowDefinitions().Append(bodyRow);
 RowDefinition foot;foot.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(foot);
 paper=Border();paper.Margin(Thickness{10,10,10,2});paper.CornerRadius(CornerRadius{8,8,8,8});paper.Background(ThemeBrush(L"CardBackgroundFillColorDefault",Windows::UI::Color{255,60,60,60}));
 body=TextBox();body.AcceptsReturn(true);body.TextWrapping(TextWrapping::Wrap);body.PlaceholderText(i18n::Tr(L"记点什么，随手捕获…"));body.FontSize(14);body.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));body.BorderThickness(Thickness{0});body.Padding(Thickness{12,10,12,10});body.VerticalAlignment(VerticalAlignment::Stretch);body.HorizontalAlignment(HorizontalAlignment::Stretch);ScrollViewer::SetVerticalScrollBarVisibility(body,ScrollBarVisibility::Auto);body.Loaded([this](auto&&,auto&&){body.Focus(FocusState::Programmatic);});
 paper.Child(body);Grid::SetRow(paper,0);root.Children().Append(paper);
 auto hint=TextBlock();hint.Text(i18n::Tr(L"Enter 存入便签 · Ctrl+Enter 存为待办 · Esc 关闭"));hint.FontSize(11);hint.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));hint.HorizontalAlignment(HorizontalAlignment::Center);hint.Margin(Thickness{0,2,0,10});
 Grid::SetRow(hint,1);root.Children().Append(hint);
 root.PreviewKeyDown([this](auto&&,Input::KeyRoutedEventArgs const& a){
  if(a.Key()==Windows::System::VirtualKey::Escape){a.Handled(true);window.DispatcherQueue().TryEnqueue([this]{owner.CloseCapture();});}
  else if(a.Key()==Windows::System::VirtualKey::Enter){a.Handled(true);Commit((GetKeyState(VK_CONTROL)&0x8000)!=0);}
 });
 window.Content(root);
 window.Closed([this](auto&&,auto&&){if(closing)return;closing=true;window.DispatcherQueue().TryEnqueue([this]{owner.CloseCapture();});});
 RECT rc{0,0,560,230};POINT cur{};GetCursorPos(&cur);
 HMONITOR hm=MonitorFromPoint(cur,MONITOR_DEFAULTTONEAREST);MONITORINFOEXW mi{sizeof(mi)};GetMonitorInfoW(hm,&mi);
 int w=mi.rcWork.right-mi.rcWork.left,h=mi.rcWork.bottom-mi.rcWork.top;
 RECT r{mi.rcWork.left+(w-560)/2,mi.rcWork.top+h/6,mi.rcWork.left+(w+560)/2,mi.rcWork.top+h/6+230};
 if(w<600||h<320)r=rc;
 SetWindowPos(hwnd,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER);
 SetWindowPos(hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
 window.Activate();
 window.DispatcherQueue().TryEnqueue([this]{body.Focus(FocusState::Programmatic);});
}
void CaptureWindow::Show(){POINT cur{};GetCursorPos(&cur);HMONITOR hm=MonitorFromPoint(cur,MONITOR_DEFAULTTONEAREST);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(hm,&mi);int x=mi.rcWork.left+((mi.rcWork.right-mi.rcWork.left)-560)/2,y=mi.rcWork.top+(mi.rcWork.bottom-mi.rcWork.top)/6;RECT c{};GetWindowRect(hwnd,&c);int w=c.right-c.left,h=c.bottom-c.top;if(hm!=MonitorFromRect(&c,MONITOR_DEFAULTTONULL))SetWindowPos(hwnd,nullptr,x,y,w,h,SWP_NOZORDER);else SetWindowPos(hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);window.Activate();body.Focus(FocusState::Programmatic);}
CaptureWindow::~CaptureWindow(){closing=true;if(IsWindow(hwnd))window.Close();}
}
