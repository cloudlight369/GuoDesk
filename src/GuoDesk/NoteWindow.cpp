#include "pch.h"
#include "NoteWindow.h"
#include "DeskWindow.h"
#include "Shell.h"
#include "I18n.h"
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
namespace guodesk {
static std::wstring NoteExePath(){wchar_t buf[MAX_PATH]{};GetModuleFileNameW(nullptr,buf,MAX_PATH);return buf;}
static Brush ResolveNoteBrush(wchar_t const* key,Windows::UI::Color fallback,bool dark){
 try{return Application::Current().Resources().Lookup(box_value(key)).as<Brush>();}catch(...){}
 struct Entry{wchar_t const* key;Windows::UI::Color light;Windows::UI::Color dark;};
 static const Entry palette[]{
  {L"NotePaperFill",{255,255,244,179},{255,63,56,34}},
  {L"TextFillColorSecondary",{255,97,97,97},{255,199,199,199}},
 };
 for(auto const& e:palette)if(!wcscmp(e.key,key))return SolidColorBrush(dark?e.dark:e.light);
 return SolidColorBrush(fallback);
}
Brush NoteWindow::ThemeBrush(wchar_t const* key,Windows::UI::Color fallback){bool dark=false;try{dark=root.ActualTheme()==ElementTheme::Dark;}catch(...){}return ResolveNoteBrush(key,fallback,dark);}
NoteWindow::NoteWindow(Controller& c):owner(c){
 window=Window();window.Title(i18n::Tr(L"GuoDesk 便签"));hwnd=shell::Handle(window);
 try{auto dir=std::filesystem::path(NoteExePath()).parent_path();window.AppWindow().SetIcon((dir/L"guodesk.ico").wstring());}catch(...){}
 window.SystemBackdrop(MicaBackdrop());
 try{auto presenter=window.AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();presenter.SetBorderAndTitleBar(true,false);window.AppWindow().IsShownInSwitchers(false);}catch(...){}
 auto& w=owner.layout.widgets;
 root=Grid();root.RowSpacing(0);
 RowDefinition head;head.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(head);
 RowDefinition bodyRow;bodyRow.Height(GridLength{1,GridUnitType::Star});root.RowDefinitions().Append(bodyRow);
 Border header;header.Padding(Thickness{12,8,12,6});header.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));
 StackPanel headRow;headRow.Orientation(Orientation::Horizontal);headRow.Spacing(8);
 FontIcon grip;grip.FontFamily(FontFamily(L"Segoe Fluent Icons"));grip.Glyph(L"\uE7C2");grip.FontSize(12);grip.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));headRow.Children().Append(grip);
 TextBlock title;title.Text(i18n::Tr(L"便签"));title.FontSize(12);title.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));headRow.Children().Append(title);
 header.Child(headRow);root.Children().Append(header);
 auto dragging=std::make_shared<bool>(false);auto dragStart=std::make_shared<POINT>();auto dragOrigin=std::make_shared<RECT>();
 auto dragTimer=root.DispatcherQueue().CreateTimer();dragTimer.Interval(std::chrono::milliseconds(16));
 auto EndDrag=[this,dragging,dragTimer](){if(!*dragging)return;*dragging=false;dragTimer.Stop();RECT r{};GetWindowRect(hwnd,&r);auto& w=owner.layout.widgets;w.noteX=r.left;w.noteY=r.top;owner.Save();};
 dragTimer.Tick([this,dragging,dragStart,dragOrigin,EndDrag](auto&&,auto&&){if(!(GetAsyncKeyState(VK_LBUTTON)&0x8000)){EndDrag();return;}POINT p{};GetCursorPos(&p);SetWindowPos(hwnd,nullptr,dragOrigin->left+p.x-dragStart->x,dragOrigin->top+p.y-dragStart->y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);});
 header.PointerPressed([this,header,dragging,dragStart,dragOrigin,dragTimer](auto&&,Input::PointerRoutedEventArgs const& a){*dragging=true;GetCursorPos(&*dragStart);GetWindowRect(hwnd,&*dragOrigin);header.CapturePointer(a.Pointer());a.Handled(true);dragTimer.Start();});
 header.PointerReleased([EndDrag](auto&&,auto&&){EndDrag();});
 header.PointerCaptureLost([EndDrag](auto&&,auto&&){EndDrag();});
 Border paper;paper.Margin(Thickness{10,0,10,10});paper.CornerRadius(CornerRadius{10,10,10,10});paper.Background(ThemeBrush(L"NotePaperFill",Windows::UI::Color{255,255,244,179}));
 body=TextBox();body.AcceptsReturn(true);body.TextWrapping(TextWrapping::Wrap);body.PlaceholderText(i18n::Tr(L"记点什么…"));body.FontSize(14);body.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));body.BorderThickness(Thickness{0});body.Padding(Thickness{12,10,12,10});body.VerticalAlignment(VerticalAlignment::Stretch);body.HorizontalAlignment(HorizontalAlignment::Stretch);ScrollViewer::SetVerticalScrollBarVisibility(body,ScrollBarVisibility::Auto);
 body.Text(w.noteText);
 paper.Child(body);Grid::SetRow(paper,1);root.Children().Append(paper);
 body.TextChanged([this](auto&&,auto&&){auto t=std::wstring(body.Text());size_t pos=0;while((pos=t.find(L"\r\n",pos))!=std::wstring::npos){t.replace(pos,2,L"\n");pos+=1;}while((pos=t.find(L'\r'))!=std::wstring::npos)t.replace(pos,1,L"\n");owner.layout.widgets.noteText=std::move(t);saveTimer.Start();});
 saveTimer=root.DispatcherQueue().CreateTimer();saveTimer.Interval(std::chrono::milliseconds(600));saveTimer.Tick([this](auto&&,auto&&){saveTimer.Stop();owner.Save();});
 window.Content(root);
 window.Closed([this](auto&&,auto&&){if(closing)return;closing=true;window.DispatcherQueue().TryEnqueue([this]{owner.CloseNote();});});
 int x=w.noteX,y=w.noteY,wd=w.noteW,ht=w.noteH;SetWindowPos(hwnd,nullptr,x,y,wd,ht,SWP_NOZORDER);
 MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
 if(y>mi.rcWork.bottom-40||y<mi.rcWork.top-20||x>mi.rcWork.right-60||x<mi.rcWork.left-40){x=std::clamp(x,(int)mi.rcWork.left,(int)mi.rcWork.right-100);y=std::clamp(y,(int)mi.rcWork.top,(int)mi.rcWork.bottom-60);SetWindowPos(hwnd,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}
 window.Activate();
}
void NoteWindow::Show(){window.Activate();}
NoteWindow::~NoteWindow(){closing=true;if(IsWindow(hwnd))window.Close();}
}
