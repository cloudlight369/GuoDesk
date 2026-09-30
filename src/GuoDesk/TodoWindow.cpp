#include "pch.h"
#include "TodoWindow.h"
#include "DeskWindow.h"
#include "Shell.h"
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
namespace guodesk {
static std::wstring TodoExePath(){wchar_t buf[MAX_PATH]{};GetModuleFileNameW(nullptr,buf,MAX_PATH);return buf;}
static Brush ResolveTodoBrush(wchar_t const* key,Windows::UI::Color fallback,bool dark){
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
Brush TodoWindow::ThemeBrush(wchar_t const* key,Windows::UI::Color fallback){bool dark=false;try{dark=root.ActualTheme()==ElementTheme::Dark;}catch(...){}return ResolveTodoBrush(key,fallback,dark);}
void TodoWindow::Add(){
 auto text=input.Text();auto added=AddTodo(owner.layout.widgets,std::wstring(text));
 if(!added)return;
 input.Text(L"");Rebuild();owner.Save();
}
void TodoWindow::Rebuild(){
 list.Children().Clear();
 auto& todos=owner.layout.widgets.todos;
 int done=0;for(auto const& t:todos)if(t.done)++done;
 count.Text(todos.empty()?L"":(std::to_wstring(done)+L"/"+std::to_wstring(todos.size())+L" 已完成"));
 if(todos.empty()){
  TextBlock empty;empty.Text(L"还没有待办，从下方添加一条");empty.FontSize(12);empty.HorizontalAlignment(HorizontalAlignment::Center);empty.Margin(Thickness{0,24,0,0});empty.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));list.Children().Append(empty);
  return;
 }
 for(auto const& t:todos){
  auto id=t.id;
  Border row;row.Padding(Thickness{10,4,4,4});row.CornerRadius(CornerRadius{6,6,6,6});row.Background(ThemeBrush(L"CardBackgroundFillColorDefault",Windows::UI::Color{255,60,60,60}));
  Grid g;ColumnDefinition c1;c1.Width(GridLength{1,GridUnitType::Star});g.ColumnDefinitions().Append(c1);ColumnDefinition c2;c2.Width(GridLength{0,GridUnitType::Auto});g.ColumnDefinitions().Append(c2);
  CheckBox box;auto label=TextBlock();label.Text(t.text);label.TextWrapping(TextWrapping::Wrap);label.FontSize(13);label.Opacity(t.done?0.45:1.0);box.Content(label);box.IsChecked(t.done);box.MinWidth(0);box.Padding(Thickness{0});box.Margin(Thickness{0,0,0,0});
  box.Checked([this,id,label](auto&&,auto&&){ToggleTodo(owner.layout.widgets,id);label.Opacity(0.45);count.Text(CountText());owner.Save();});
  box.Unchecked([this,id,label](auto&&,auto&&){ToggleTodo(owner.layout.widgets,id);label.Opacity(1.0);count.Text(CountText());owner.Save();});
  Grid::SetColumn(box,0);g.Children().Append(box);
  Button del;del.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));del.BorderThickness(Thickness{0});del.Padding(Thickness{6,4,6,4});del.Margin(Thickness{0,2,0,0});
  FontIcon trash;trash.FontFamily(FontFamily(L"Segoe Fluent Icons"));trash.Glyph(L"\uE74D");trash.FontSize(12);trash.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,160,160,160}));del.Content(trash);
  del.Click([this,id](auto&&,auto&&){RemoveTodo(owner.layout.widgets,id);Rebuild();owner.Save();});
  Grid::SetColumn(del,1);g.Children().Append(del);
  row.Child(g);list.Children().Append(row);
 }
}
std::wstring TodoWindow::CountText(){
 auto& todos=owner.layout.widgets.todos;
 if(todos.empty())return L"";
 int done=0;for(auto const& t:todos)if(t.done)++done;
 return std::to_wstring(done)+L"/"+std::to_wstring(todos.size())+L" 已完成";
}
TodoWindow::TodoWindow(Controller& c):owner(c){
 window=Window();window.Title(L"GuoDesk 待办");hwnd=shell::Handle(window);
 try{auto dir=std::filesystem::path(TodoExePath()).parent_path();window.AppWindow().SetIcon((dir/L"guodesk.ico").wstring());}catch(...){}
 window.SystemBackdrop(MicaBackdrop());
 try{auto presenter=window.AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();presenter.SetBorderAndTitleBar(true,false);window.AppWindow().IsShownInSwitchers(false);}catch(...){}
 auto& w=owner.layout.widgets;
 root=Grid();root.Padding(Thickness{0,0,0,0});
 RowDefinition head;head.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(head);
 RowDefinition bodyRow;bodyRow.Height(GridLength{1,GridUnitType::Star});root.RowDefinitions().Append(bodyRow);
 RowDefinition foot;foot.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(foot);
 Border header;header.Padding(Thickness{12,8,12,6});header.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));
 Grid headGrid;ColumnDefinition hc1;hc1.Width(GridLength{1,GridUnitType::Star});headGrid.ColumnDefinitions().Append(hc1);ColumnDefinition hc2;hc2.Width(GridLength{0,GridUnitType::Auto});headGrid.ColumnDefinitions().Append(hc2);
 StackPanel headLeft;headLeft.Orientation(Orientation::Horizontal);headLeft.Spacing(8);
 FontIcon grip;grip.FontFamily(FontFamily(L"Segoe Fluent Icons"));grip.Glyph(L"\uE7C2");grip.FontSize(12);grip.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));headLeft.Children().Append(grip);
 TextBlock title;title.Text(L"待办");title.FontSize(12);title.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));headLeft.Children().Append(title);
 Grid::SetColumn(headLeft,0);headGrid.Children().Append(headLeft);
 count=TextBlock();count.FontSize(12);count.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));count.VerticalAlignment(VerticalAlignment::Center);Grid::SetColumn(count,1);headGrid.Children().Append(count);
 header.Child(headGrid);root.Children().Append(header);
 auto dragging=std::make_shared<bool>(false);auto dragStart=std::make_shared<POINT>();auto dragOrigin=std::make_shared<RECT>();
 auto dragTimer=root.DispatcherQueue().CreateTimer();dragTimer.Interval(std::chrono::milliseconds(16));
 auto EndDrag=[this,dragging,dragTimer](){if(!*dragging)return;*dragging=false;dragTimer.Stop();RECT r{};GetWindowRect(hwnd,&r);auto& w=owner.layout.widgets;w.todoX=r.left;w.todoY=r.top;owner.Save();};
 dragTimer.Tick([this,dragging,dragStart,dragOrigin,EndDrag](auto&&,auto&&){if(!(GetAsyncKeyState(VK_LBUTTON)&0x8000)){EndDrag();return;}POINT p{};GetCursorPos(&p);SetWindowPos(hwnd,nullptr,dragOrigin->left+p.x-dragStart->x,dragOrigin->top+p.y-dragStart->y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);});
 header.PointerPressed([this,header,dragging,dragStart,dragOrigin,dragTimer](auto&&,Input::PointerRoutedEventArgs const& a){*dragging=true;GetCursorPos(&*dragStart);GetWindowRect(hwnd,&*dragOrigin);header.CapturePointer(a.Pointer());a.Handled(true);dragTimer.Start();});
 header.PointerReleased([EndDrag](auto&&,auto&&){EndDrag();});
 header.PointerCaptureLost([EndDrag](auto&&,auto&&){EndDrag();});
 auto scroll=ScrollViewer();scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);scroll.Padding(Thickness{8,0,8,0});
 list=StackPanel();list.Spacing(4);scroll.Content(list);Grid::SetRow(scroll,1);root.Children().Append(scroll);
 auto bar=Grid();bar.Padding(Thickness{8,6,8,10});ColumnDefinition fc1;fc1.Width(GridLength{1,GridUnitType::Star});bar.ColumnDefinitions().Append(fc1);ColumnDefinition fc2;fc2.Width(GridLength{0,GridUnitType::Auto});bar.ColumnDefinitions().Append(fc2);
 input=TextBox();input.PlaceholderText(L"添加待办，回车确认");input.FontSize(13);input.Margin(Thickness{0,0,8,0});Grid::SetColumn(input,0);bar.Children().Append(input);
 auto addBtn=Button();addBtn.Content(box_value(L"添加"));try{addBtn.Style(Application::Current().Resources().Lookup(box_value(L"AccentButtonStyle")).as<Style>());}catch(...){}
 addBtn.Click([this](auto&&,auto&&){Add();});Grid::SetColumn(addBtn,1);bar.Children().Append(addBtn);
 input.KeyDown([this](auto&&,Input::KeyRoutedEventArgs const& a){if(a.Key()==winrt::Windows::System::VirtualKey::Enter)Add();});
 Grid::SetRow(bar,2);root.Children().Append(bar);
 window.Content(root);
 window.Closed([this](auto&&,auto&&){if(closing)return;closing=true;window.DispatcherQueue().TryEnqueue([this]{owner.CloseTodo();});});
 int x=w.todoX,y=w.todoY,wd=w.todoW,ht=w.todoH;SetWindowPos(hwnd,nullptr,x,y,wd,ht,SWP_NOZORDER);
 MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
 if(y>mi.rcWork.bottom-40||y<mi.rcWork.top-20||x>mi.rcWork.right-60||x<mi.rcWork.left-40){x=std::clamp(x,(int)mi.rcWork.left,(int)mi.rcWork.right-100);y=std::clamp(y,(int)mi.rcWork.top,(int)mi.rcWork.bottom-60);SetWindowPos(hwnd,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}
 Rebuild();
 window.Activate();
}
void TodoWindow::Show(){Rebuild();window.Activate();}
TodoWindow::~TodoWindow(){closing=true;if(IsWindow(hwnd))window.Close();}
}
