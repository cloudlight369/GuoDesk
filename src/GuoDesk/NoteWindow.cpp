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
  {L"TextFillColorSecondary",{255,97,97,97},{255,199,199,199}},
 };
 for(auto const& e:palette)if(!wcscmp(e.key,key))return SolidColorBrush(dark?e.dark:e.light);
 return SolidColorBrush(fallback);
}
Brush NoteWindow::ThemeBrush(wchar_t const* key,Windows::UI::Color fallback){bool dark=false;try{dark=root.ActualTheme()==ElementTheme::Dark;}catch(...){}return ResolveNoteBrush(key,fallback,dark);}
Brush NoteWindow::AccentBrush(){
 try{return Application::Current().Resources().Lookup(box_value(L"AccentFillColorDefaultBrush")).as<Brush>();}catch(...){}
 return SolidColorBrush(Windows::UI::Color{255,0,120,212});
}
Brush NoteWindow::PaperBrush(int color){
 struct Entry{Windows::UI::Color light,dark;};
 static const Entry papers[]{
  {{255,255,244,179},{255,63,56,34}},
  {{255,255,219,214},{255,72,44,46}},
  {{255,217,240,217},{255,40,58,44}},
  {{255,212,231,249},{255,36,50,64}},
  {{255,234,222,248},{255,52,44,66}},
  {{255,244,244,244},{255,56,56,56}},
 };
 color=std::clamp(color,0,5);bool dark=false;try{dark=root.ActualTheme()==ElementTheme::Dark;}catch(...){}
 auto& e=papers[color];return SolidColorBrush(dark?e.dark:e.light);
}
void NoteWindow::ApplyPaper(){paper.Background(PaperBrush(ActiveNote(owner.layout.widgets)->color));}
void NoteWindow::ApplyTop(){
 auto& w=owner.layout.widgets;
 if(IsWindow(hwnd))SetWindowPos(hwnd,w.noteTop?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
 if(pinGlyph){pinGlyph.Glyph(w.noteTop?L"\uE718":L"\uE77A");pinGlyph.Foreground(w.noteTop?AccentBrush():ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));}
}
void NoteWindow::LoadPage(){
 auto& w=owner.layout.widgets;auto* p=ActiveNote(w);
 body.Text(p->text);ApplyPaper();
 pager.Text(std::to_wstring(w.notePage+1)+L"/"+std::to_wstring(w.pages.size()));
}
NoteWindow::NoteWindow(Controller& c):owner(c){
 window=Window();window.Title(i18n::Tr(L"GuoDesk 便签"));hwnd=shell::Handle(window);
 try{auto dir=std::filesystem::path(NoteExePath()).parent_path();window.AppWindow().SetIcon((dir/L"guodesk.ico").wstring());}catch(...){}
 window.SystemBackdrop(MicaBackdrop());
 try{auto presenter=window.AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();presenter.SetBorderAndTitleBar(true,false);window.AppWindow().IsShownInSwitchers(false);}catch(...){}
 auto& w=owner.layout.widgets;
 root=Grid();root.RowSpacing(0);
 RowDefinition head;head.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(head);
 RowDefinition bodyRow;bodyRow.Height(GridLength{1,GridUnitType::Star});root.RowDefinitions().Append(bodyRow);
 RowDefinition foot;foot.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(foot);
 Border header;header.Padding(Thickness{12,8,8,6});header.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));
 Grid headGrid;ColumnDefinition hc1;hc1.Width(GridLength{1,GridUnitType::Star});headGrid.ColumnDefinitions().Append(hc1);ColumnDefinition hc2;hc2.Width(GridLength{0,GridUnitType::Auto});headGrid.ColumnDefinitions().Append(hc2);
 StackPanel headRow;headRow.Orientation(Orientation::Horizontal);headRow.Spacing(8);
 FontIcon grip;grip.FontFamily(FontFamily(L"Segoe Fluent Icons"));grip.Glyph(L"\uE7C2");grip.FontSize(ScaledFont(owner.layout.settings.textSize,12));grip.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));headRow.Children().Append(grip);
 TextBlock title;title.Text(i18n::Tr(L"便签"));title.FontSize(ScaledFont(owner.layout.settings.textSize,12));title.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));headRow.Children().Append(title);
 Grid::SetColumn(headRow,0);headGrid.Children().Append(headRow);
 auto glyphBtn=[this](wchar_t const* glyph,int size){Button b;b.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));b.BorderThickness(Thickness{0});b.Padding(Thickness{6,2,6,2});b.MinWidth(0);FontIcon ic;ic.FontFamily(FontFamily(L"Segoe Fluent Icons"));ic.Glyph(glyph);ic.FontSize(ScaledFont(owner.layout.settings.textSize,size));ic.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));b.Content(ic);return b;};
 pin=glyphBtn(L"\uE77A",12);pinGlyph=pin.Content().as<FontIcon>();pin.VerticalAlignment(VerticalAlignment::Center);pin.Click([this](auto&&,auto&&){auto& w=owner.layout.widgets;w.noteTop=!w.noteTop;ApplyTop();owner.Save();});
 Grid::SetColumn(pin,1);headGrid.Children().Append(pin);
 header.Child(headGrid);root.Children().Append(header);
 auto dragging=std::make_shared<bool>(false);auto dragStart=std::make_shared<POINT>();auto dragOrigin=std::make_shared<RECT>();
 auto dragTimer=root.DispatcherQueue().CreateTimer();dragTimer.Interval(std::chrono::milliseconds(16));
 auto EndDrag=[this,dragging,dragTimer](){if(!*dragging)return;*dragging=false;dragTimer.Stop();RECT r{};GetWindowRect(hwnd,&r);auto& w=owner.layout.widgets;w.noteX=r.left;w.noteY=r.top;MONITORINFOEXW mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);w.noteMon=mi.szDevice;w.noteMX=r.left-mi.rcWork.left;w.noteMY=r.top-mi.rcWork.top;owner.Save();};
 dragTimer.Tick([this,dragging,dragStart,dragOrigin,EndDrag](auto&&,auto&&){if(!(GetAsyncKeyState(VK_LBUTTON)&0x8000)){EndDrag();return;}POINT p{};GetCursorPos(&p);SetWindowPos(hwnd,nullptr,dragOrigin->left+p.x-dragStart->x,dragOrigin->top+p.y-dragStart->y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);});
 header.PointerPressed([this,header,dragging,dragStart,dragOrigin,dragTimer](auto&&,Input::PointerRoutedEventArgs const& a){*dragging=true;GetCursorPos(&*dragStart);GetWindowRect(hwnd,&*dragOrigin);header.CapturePointer(a.Pointer());a.Handled(true);dragTimer.Start();});
 header.PointerReleased([EndDrag](auto&&,auto&&){EndDrag();});
 header.PointerCaptureLost([EndDrag](auto&&,auto&&){EndDrag();});
 paper=Border();paper.Margin(Thickness{10,0,10,4});paper.CornerRadius(CornerRadius{10,10,10,10});paper.Background(ThemeBrush(L"NotePaperFill",Windows::UI::Color{255,255,244,179}));
 body=TextBox();body.AcceptsReturn(true);body.TextWrapping(TextWrapping::Wrap);body.PlaceholderText(i18n::Tr(L"记点什么…"));body.FontSize(ScaledFont(owner.layout.settings.textSize,14));body.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));body.BorderThickness(Thickness{0});body.Padding(Thickness{12,10,12,10});body.VerticalAlignment(VerticalAlignment::Stretch);body.HorizontalAlignment(HorizontalAlignment::Stretch);ScrollViewer::SetVerticalScrollBarVisibility(body,ScrollBarVisibility::Auto);
 paper.Child(body);Grid::SetRow(paper,1);root.Children().Append(paper);
 body.TextChanged([this](auto&&,auto&&){auto t=std::wstring(body.Text());size_t pos=0;while((pos=t.find(L"\r\n",pos))!=std::wstring::npos){t.replace(pos,2,L"\n");pos+=1;}while((pos=t.find(L'\r'))!=std::wstring::npos)t.replace(pos,1,L"\n");if(auto* p=ActiveNote(owner.layout.widgets))p->text=std::move(t);saveTimer.Start();});
 saveTimer=root.DispatcherQueue().CreateTimer();saveTimer.Interval(std::chrono::milliseconds(600));saveTimer.Tick([this](auto&&,auto&&){saveTimer.Stop();owner.Save();});
 auto footGrid=Grid();footGrid.Padding(Thickness{10,2,10,8});
 ColumnDefinition f1;f1.Width(GridLength{1,GridUnitType::Star});footGrid.ColumnDefinitions().Append(f1);ColumnDefinition f2;f2.Width(GridLength{0,GridUnitType::Auto});footGrid.ColumnDefinitions().Append(f2);
 auto nav=StackPanel();nav.Orientation(Orientation::Horizontal);nav.Spacing(2);nav.VerticalAlignment(VerticalAlignment::Center);
 auto prevBtn=glyphBtn(L"\uE76B",11);prevBtn.Click([this](auto&&,auto&&){auto& w=owner.layout.widgets;ActiveNote(w);if(w.notePage>0){--w.notePage;LoadPage();saveTimer.Start();}});
 pager=TextBlock();pager.FontSize(ScaledFont(owner.layout.settings.textSize,11));pager.VerticalAlignment(VerticalAlignment::Center);pager.Margin(Thickness{4,0,4,0});pager.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));
 auto nextBtn=glyphBtn(L"\uE76C",11);nextBtn.Click([this](auto&&,auto&&){auto& w=owner.layout.widgets;ActiveNote(w);if(w.notePage+1<static_cast<int>(w.pages.size())){++w.notePage;LoadPage();saveTimer.Start();}});
 nav.Children().Append(prevBtn);nav.Children().Append(pager);nav.Children().Append(nextBtn);
 Grid::SetColumn(nav,0);footGrid.Children().Append(nav);
 auto ops=StackPanel();ops.Orientation(Orientation::Horizontal);ops.Spacing(2);ops.VerticalAlignment(VerticalAlignment::Center);
 auto colorBtn=glyphBtn(L"\uE790",12);colorBtn.Click([this](auto&&,auto&&){auto* p=ActiveNote(owner.layout.widgets);p->color=(p->color+1)%6;ApplyPaper();saveTimer.Start();});
 auto delPageBtn=glyphBtn(L"\uE74D",12);delPageBtn.Click([this](auto&&,auto&&){RemoveNotePage(owner.layout.widgets);LoadPage();saveTimer.Start();});
 auto addBtn=glyphBtn(L"\uE710",12);addBtn.Click([this](auto&&,auto&&){AddNotePage(owner.layout.widgets);LoadPage();saveTimer.Start();});
 ops.Children().Append(colorBtn);ops.Children().Append(delPageBtn);ops.Children().Append(addBtn);
 Grid::SetColumn(ops,1);footGrid.Children().Append(ops);
 Grid::SetRow(footGrid,2);root.Children().Append(footGrid);
 window.Content(root);
 window.Closed([this](auto&&,auto&&){if(closing)return;closing=true;window.DispatcherQueue().TryEnqueue([this]{owner.CloseNote();});});
 int x=w.noteX,y=w.noteY,wd=w.noteW,ht=w.noteH;SetWindowPos(hwnd,nullptr,x,y,wd,ht,SWP_NOZORDER);
 MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
 if(y>mi.rcWork.bottom-40||y<mi.rcWork.top-20||x>mi.rcWork.right-60||x<mi.rcWork.left-40){x=std::clamp(x,(int)mi.rcWork.left,(int)mi.rcWork.right-100);y=std::clamp(y,(int)mi.rcWork.top,(int)mi.rcWork.bottom-60);SetWindowPos(hwnd,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}
 LoadPage();ApplyTop();
 window.Activate();
}
void NoteWindow::Show(){window.Activate();}
NoteWindow::~NoteWindow(){closing=true;if(IsWindow(hwnd))window.Close();}
}
