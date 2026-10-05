#include "pch.h"
#include "TodoWindow.h"
#include "DeskWindow.h"
#include "Shell.h"
#include "I18n.h"
#include <winrt/Microsoft.UI.Xaml.Documents.h>
#include <winrt/Windows.UI.Text.h>
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
namespace guodesk {
static std::wstring TodoExePath(){wchar_t buf[MAX_PATH]{};GetModuleFileNameW(nullptr,buf,MAX_PATH);return buf;}
static wchar_t const* kRepeatName340[]={L"不重复",L"每天",L"每周",L"每两周",L"每月"};
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
 std::erase_if(selected,[&](auto const& id){return !std::any_of(todos.begin(),todos.end(),[&](auto const& t){return t.id==id;});});
 if(selBar){selBar.Visibility(selected.empty()?Visibility::Collapsed:Visibility::Visible);if(selCount)selCount.Text(i18n::TrF(L"已选 {0} 项",{std::to_wstring(selected.size())}));}
 int done=0;for(auto const& t:todos)if(t.done)++done;
 if(clearDone)clearDone.IsEnabled(done>0);
 count.Text(todos.empty()?L"":i18n::TrF(L"{0}/{1} 已完成",{std::to_wstring(done),std::to_wstring(todos.size())}));
 if(todos.empty()){
  TextBlock empty;empty.Text(i18n::Tr(L"还没有待办，从下方添加一条"));empty.FontSize(ScaledFont(owner.layout.settings.textSize,12));empty.HorizontalAlignment(HorizontalAlignment::Center);empty.Margin(Thickness{0,24,0,0});empty.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));list.Children().Append(empty);
  return;
 }
 std::wstring q=query?std::wstring(query.Text()):std::wstring();
 int shown=0;
 for(auto const& t:todos){
  if(!TodoMatchesFilter(t,filterKind)||!TodoMatchesQuery(t,q))continue;
  ++shown;
  auto id=t.id;
  Border row;row.Padding(Thickness{10,4,4,4});row.CornerRadius(CornerRadius{6,6,6,6});
  if(std::find(selected.begin(),selected.end(),id)!=selected.end())row.Background(SolidColorBrush(Windows::UI::Color{255,94,148,208}));
  else row.Background(ThemeBrush(L"CardBackgroundFillColorDefault",Windows::UI::Color{255,60,60,60}));
  Grid g;ColumnDefinition c0;c0.Width(GridLength{0,GridUnitType::Auto});g.ColumnDefinitions().Append(c0);ColumnDefinition c1;c1.Width(GridLength{1,GridUnitType::Star});g.ColumnDefinitions().Append(c1);ColumnDefinition c2;c2.Width(GridLength{0,GridUnitType::Auto});g.ColumnDefinitions().Append(c2);ColumnDefinition c3;c3.Width(GridLength{0,GridUnitType::Auto});g.ColumnDefinitions().Append(c3);
  if(t.flag){unsigned rgb=TodoFlagRGB(t.flag);Border dot;dot.Width(8);dot.Height(8);dot.CornerRadius(CornerRadius{4,4,4,4});dot.Background(SolidColorBrush(Windows::UI::Color{255,static_cast<BYTE>((rgb>>16)&0xFF),static_cast<BYTE>((rgb>>8)&0xFF),static_cast<BYTE>(rgb&0xFF)}));dot.VerticalAlignment(VerticalAlignment::Center);dot.Margin(Thickness{2,0,6,0});Grid::SetColumn(dot,0);g.Children().Append(dot);}
  CheckBox box;auto label=TextBlock();
  for(auto const& seg:ParseInlineMarkdown(t.text)){
   winrt::Microsoft::UI::Xaml::Documents::Run r;r.Text(seg.text);
   if(seg.style==1)r.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
   else if(seg.style==2)r.TextDecorations(winrt::Windows::UI::Text::TextDecorations::Strikethrough);
   else if(seg.style==3)r.FontFamily(FontFamily(L"Consolas"));
   else if(seg.style==4)r.Foreground(SolidColorBrush(Windows::UI::Color{255,0,120,212}));
   label.Inlines().Append(r);
  }
  label.TextWrapping(TextWrapping::Wrap);label.FontSize(ScaledFont(owner.layout.settings.textSize,13));label.Opacity(t.done?0.45:1.0);box.Content(label);box.IsChecked(t.done);box.MinWidth(0);box.Padding(Thickness{0});box.Margin(Thickness{0,0,0,0});
  box.Checked([this,id,label](auto&&,auto&&){if(ToggleTodo(owner.layout.widgets,id)==2){Rebuild();owner.Save();return;}label.Opacity(0.45);count.Text(CountText());owner.Save();});
  box.Unchecked([this,id,label](auto&&,auto&&){ToggleTodo(owner.layout.widgets,id);label.Opacity(1.0);count.Text(CountText());owner.Save();});
  Grid::SetColumn(box,1);g.Children().Append(box);
  Button dueBtn;dueBtn.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));dueBtn.BorderThickness(Thickness{0});dueBtn.Padding(Thickness{6,4,6,4});dueBtn.Margin(Thickness{0,2,0,0});dueBtn.MinWidth(0);
  auto dueRow=StackPanel();dueRow.Orientation(Orientation::Horizontal);dueRow.Spacing(4);
  FontIcon cal;cal.FontFamily(FontFamily(L"Segoe Fluent Icons"));cal.Glyph(L"\uE787");cal.FontSize(ScaledFont(owner.layout.settings.textSize,12));cal.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,160,160,160}));
  TextBlock dueLabel;dueLabel.FontSize(ScaledFont(owner.layout.settings.textSize,11));
  if(t.due||t.repeat){std::wstring s=t.repeat?std::wstring(i18n::Tr(kRepeatName340[t.repeat])):std::wstring();if(t.due){auto d=DueText(t.due);s=s.empty()?d:s+L" · "+d;}dueLabel.Text(s);if(DueReached(t.due)&&!t.done){auto red=SolidColorBrush(Windows::UI::Color{255,232,17,35});cal.Foreground(red);dueLabel.Foreground(red);dueLabel.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());}}
  dueRow.Children().Append(cal);dueRow.Children().Append(dueLabel);dueBtn.Content(dueRow);
  MenuFlyout mf;
  auto mk=[&](wchar_t const* key,int days){MenuFlyoutItem mi;mi.Text(i18n::Tr(key));mi.Click([this,id,days](auto&&,auto&&){SetTodoDue(owner.layout.widgets,id,days<0?0:DueFromOffset(days));Rebuild();owner.Save();});mf.Items().Append(mi);};
  mk(L"今天",0);mk(L"明天",1);mk(L"下周",7);
  MenuFlyoutSeparator sep;mf.Items().Append(sep);
  mk(L"清除截止",-1);
  dueBtn.Flyout(mf);
  Grid::SetColumn(dueBtn,2);g.Children().Append(dueBtn);
  Button del;del.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));del.BorderThickness(Thickness{0});del.Padding(Thickness{6,4,6,4});del.Margin(Thickness{0,2,0,0});
  FontIcon trash;trash.FontFamily(FontFamily(L"Segoe Fluent Icons"));trash.Glyph(L"\uE74D");trash.FontSize(ScaledFont(owner.layout.settings.textSize,12));trash.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,160,160,160}));del.Content(trash);
  del.Click([this,id](auto&&,auto&&){RemoveTodo(owner.layout.widgets,id);Rebuild();owner.Save();});
  Grid::SetColumn(del,3);g.Children().Append(del);
  MenuFlyout flagMenu;
  auto mkf=[&](wchar_t const* key,int f){MenuFlyoutItem mi;mi.Text(i18n::Tr(key));if(f){unsigned rgb=TodoFlagRGB(f);FontIcon sq;sq.Glyph(L"\u25A0");sq.FontFamily(FontFamily(L"Segoe UI Symbol"));sq.FontSize(12);sq.Foreground(SolidColorBrush(Windows::UI::Color{255,static_cast<BYTE>((rgb>>16)&0xFF),static_cast<BYTE>((rgb>>8)&0xFF),static_cast<BYTE>(rgb&0xFF)}));mi.Icon(sq);}mi.Click([this,id,f](auto&&,auto&&){for(auto& t:owner.layout.widgets.todos)if(t.id==id)t.flag=f;Rebuild();owner.Save();});flagMenu.Items().Append(mi);};
  mkf(L"无标记",0);mkf(L"红色 · 紧急",1);mkf(L"黄色 · 重要",2);mkf(L"绿色 · 低",3);
  MenuFlyoutSeparator rsep;flagMenu.Items().Append(rsep);
  MenuFlyoutItem selMi;selMi.Text(i18n::Tr(std::find(selected.begin(),selected.end(),id)!=selected.end()?L"取消选择":L"选择/取消选择"));selMi.Click([this,id](auto&&,auto&&){auto it=std::find(selected.begin(),selected.end(),id);if(it!=selected.end())selected.erase(it);else selected.push_back(id);Rebuild();});flagMenu.Items().Append(selMi);
  MenuFlyoutSubItem repSub;repSub.Text(i18n::Tr(L"重复"));ToolTipService::SetToolTip(repSub,box_value(i18n::Tr(L"周期待办完成后会自动顺延")));
  auto mkr=[&](int r){MenuFlyoutItem mi;mi.Text(i18n::Tr(kRepeatName340[r]));if(r){FontIcon ri;ri.FontFamily(FontFamily(L"Segoe Fluent Icons"));ri.Glyph(L"\uE72C");ri.FontSize(12);mi.Icon(ri);}mi.Click([this,id,r](auto&&,auto&&){for(auto& t:owner.layout.widgets.todos)if(t.id==id){t.repeat=r;if(r>0&&t.due==0)t.due=DueFromOffset(r==1?0:r==2?7:r==3?14:30);}Rebuild();owner.Save();});repSub.Items().Append(mi);};
  mkr(0);mkr(1);mkr(2);mkr(3);mkr(4);
  flagMenu.Items().Append(repSub);
  row.ContextFlyout(flagMenu);
  row.Child(g);list.Children().Append(row);
 }
 if(!shown){
  TextBlock none;none.Text(i18n::Tr(L"没有符合当前筛选的待办"));none.FontSize(ScaledFont(owner.layout.settings.textSize,12));none.HorizontalAlignment(HorizontalAlignment::Center);none.Margin(Thickness{0,24,0,0});none.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));list.Children().Append(none);
 }
}
std::wstring TodoWindow::CountText(){
 auto& todos=owner.layout.widgets.todos;
 if(todos.empty())return L"";
 int done=0;for(auto const& t:todos)if(t.done)++done;
 return i18n::TrF(L"{0}/{1} 已完成",{std::to_wstring(done),std::to_wstring(todos.size())});
}
TodoWindow::TodoWindow(Controller& c):owner(c){
 window=Window();window.Title(i18n::Tr(L"GuoDesk 待办"));hwnd=shell::Handle(window);
 try{auto dir=std::filesystem::path(TodoExePath()).parent_path();window.AppWindow().SetIcon((dir/L"guodesk.ico").wstring());}catch(...){}
 window.SystemBackdrop(MakeBackdrop(owner.layout.settings.backdrop));
 try{auto presenter=window.AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();presenter.SetBorderAndTitleBar(true,false);window.AppWindow().IsShownInSwitchers(false);}catch(...){}
 auto& w=owner.layout.widgets;
 root=Grid();root.Padding(Thickness{0,0,0,0});
 RowDefinition head;head.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(head);
 RowDefinition bodyRow;bodyRow.Height(GridLength{1,GridUnitType::Star});root.RowDefinitions().Append(bodyRow);
 RowDefinition toolRow;toolRow.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(toolRow);
 RowDefinition foot;foot.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(foot);
 Border header;header.Padding(Thickness{12,8,12,6});header.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));
 Grid headGrid;ColumnDefinition hc1;hc1.Width(GridLength{1,GridUnitType::Star});headGrid.ColumnDefinitions().Append(hc1);ColumnDefinition hc2;hc2.Width(GridLength{0,GridUnitType::Auto});headGrid.ColumnDefinitions().Append(hc2);
 StackPanel headLeft;headLeft.Orientation(Orientation::Horizontal);headLeft.Spacing(8);
 FontIcon grip;grip.FontFamily(FontFamily(L"Segoe Fluent Icons"));grip.Glyph(L"\uE7C2");grip.FontSize(ScaledFont(owner.layout.settings.textSize,12));grip.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));headLeft.Children().Append(grip);
 TextBlock title;title.Text(i18n::Tr(L"待办"));title.FontSize(ScaledFont(owner.layout.settings.textSize,12));title.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));headLeft.Children().Append(title);
 Grid::SetColumn(headLeft,0);headGrid.Children().Append(headLeft);
 count=TextBlock();count.FontSize(ScaledFont(owner.layout.settings.textSize,12));count.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));count.VerticalAlignment(VerticalAlignment::Center);Grid::SetColumn(count,1);headGrid.Children().Append(count);
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
 auto tool=Grid();tool.Padding(Thickness{10,2,10,2});ColumnDefinition tc0;tc0.Width(GridLength{0,GridUnitType::Auto});tool.ColumnDefinitions().Append(tc0);ColumnDefinition tc1;tc1.Width(GridLength{1,GridUnitType::Star});tool.ColumnDefinitions().Append(tc1);ColumnDefinition tc2;tc2.Width(GridLength{0,GridUnitType::Auto});tool.ColumnDefinitions().Append(tc2);
 filter=ComboBox();filter.MinWidth(88);for(wchar_t const* p:{L"全部",L"未完成",L"已完成",L"逾期",L"重复"}){ComboBoxItem it;it.Content(box_value(i18n::Tr(p)));filter.Items().Append(it);}
 filter.SelectedIndex(0);
 filter.SelectionChanged([this](auto&&,auto&&){filterKind=filter.SelectedIndex();if(filterKind<0||filterKind>4)filterKind=0;Rebuild();});
 Grid::SetColumn(filter,0);tool.Children().Append(filter);
 query=TextBox();query.PlaceholderText(i18n::Tr(L"搜索待办…"));query.FontSize(ScaledFont(owner.layout.settings.textSize,12));query.Margin(Thickness{8,0,8,0});
 query.TextChanged([this](auto&&,auto&&){Rebuild();});
 Grid::SetColumn(query,1);tool.Children().Append(query);
 clearDone=Button();clearDone.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));clearDone.BorderThickness(Thickness{0});clearDone.Padding(Thickness{6,2,6,2});clearDone.MinWidth(0);
 auto clearRow=StackPanel();clearRow.Orientation(Orientation::Horizontal);clearRow.Spacing(4);
 FontIcon brushIcon;brushIcon.FontFamily(FontFamily(L"Segoe Fluent Icons"));brushIcon.Glyph(L"\uE74D");brushIcon.FontSize(ScaledFont(owner.layout.settings.textSize,11));TextBlock clearLabel;clearLabel.Text(i18n::Tr(L"清除已完成"));clearLabel.FontSize(ScaledFont(owner.layout.settings.textSize,11));clearRow.Children().Append(brushIcon);clearRow.Children().Append(clearLabel);
 clearDone.Content(clearRow);
 clearDone.Click([this](auto&&,auto&&){int total=0;for(auto const& t:owner.layout.widgets.todos)if(t.done)++total;if(!total)return;owner.PushUndo(i18n::Tr(L"清除已完成"));int n=ClearDoneTodos(owner.layout.widgets);Rebuild();owner.Save();owner.Toast(i18n::Tr(L"待办"),i18n::TrF(L"已清除 {0} 条已完成。",{std::to_wstring(n)}));});
 Grid::SetColumn(clearDone,2);tool.Children().Append(clearDone);
 selBar=Border();selBar.Padding(Thickness{10,2,10,2});selBar.Visibility(Visibility::Collapsed);
 auto selRow=StackPanel();selRow.Orientation(Orientation::Horizontal);selRow.Spacing(8);
 selCount=TextBlock();selCount.FontSize(ScaledFont(owner.layout.settings.textSize,12));selCount.VerticalAlignment(VerticalAlignment::Center);selRow.Children().Append(selCount);
 auto selDel=Button();selDel.Content(box_value(i18n::Tr(L"删除所选")));selDel.FontSize(ScaledFont(owner.layout.settings.textSize,12));
 selDel.Click([this](auto&&,auto&&){if(selected.empty())return;owner.PushUndo(i18n::Tr(L"删除所选待办"));int n=RemoveTodos(owner.layout.widgets,selected);selected.clear();Rebuild();owner.Save();if(n)owner.Toast(i18n::Tr(L"待办"),i18n::TrF(L"已删除 {0} 条。",{std::to_wstring(n)}));});
 selRow.Children().Append(selDel);
 auto selNone=Button();selNone.Content(box_value(i18n::Tr(L"取消选择")));selNone.FontSize(ScaledFont(owner.layout.settings.textSize,12));
 selNone.Click([this](auto&&,auto&&){selected.clear();Rebuild();});
 selRow.Children().Append(selNone);
 selBar.Child(selRow);
 auto tools=StackPanel();tools.Children().Append(selBar);tools.Children().Append(tool);
 Grid::SetRow(tools,2);root.Children().Append(tools);
 auto bar=Grid();bar.Padding(Thickness{8,6,8,10});ColumnDefinition fc1;fc1.Width(GridLength{1,GridUnitType::Star});bar.ColumnDefinitions().Append(fc1);ColumnDefinition fc2;fc2.Width(GridLength{0,GridUnitType::Auto});bar.ColumnDefinitions().Append(fc2);
 input=TextBox();input.PlaceholderText(i18n::Tr(L"添加待办，回车确认"));input.FontSize(ScaledFont(owner.layout.settings.textSize,13));input.Margin(Thickness{0,0,8,0});Grid::SetColumn(input,0);bar.Children().Append(input);
 auto addBtn=Button();addBtn.Content(box_value(i18n::Tr(L"添加")));try{addBtn.Style(Application::Current().Resources().Lookup(box_value(L"AccentButtonStyle")).as<Style>());}catch(...){}
 addBtn.Click([this](auto&&,auto&&){Add();});Grid::SetColumn(addBtn,1);bar.Children().Append(addBtn);
 input.KeyDown([this](auto&&,Input::KeyRoutedEventArgs const& a){if(a.Key()==winrt::Windows::System::VirtualKey::Enter)Add();});
 Grid::SetRow(bar,3);root.Children().Append(bar);
 window.Content(root);
 window.Closed([this](auto&&,auto&&){if(closing)return;closing=true;window.DispatcherQueue().TryEnqueue([this]{owner.CloseTodo();});});
 int x=w.todoX,y=w.todoY,wd=w.todoW,ht=w.todoH;SetWindowPos(hwnd,nullptr,x,y,wd,ht,SWP_NOZORDER);
 MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
 if(y>mi.rcWork.bottom-40||y<mi.rcWork.top-20||x>mi.rcWork.right-60||x<mi.rcWork.left-40){x=std::clamp(x,(int)mi.rcWork.left,(int)mi.rcWork.right-100);y=std::clamp(y,(int)mi.rcWork.top,(int)mi.rcWork.bottom-60);SetWindowPos(hwnd,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}
 Rebuild();
 window.Activate();
}
void TodoWindow::Show(){Rebuild();window.Activate();}
TodoWindow::~TodoWindow(){closing=true;try{window.Closed(nullptr);}catch(...){}if(IsWindow(hwnd))window.Close();}
}
