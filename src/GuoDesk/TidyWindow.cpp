#include "pch.h"
#include "TidyWindow.h"
#include "DeskWindow.h"
#include "Shell.h"
#include "I18n.h"
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
namespace guodesk {
static std::wstring TidyExePath(){wchar_t buf[MAX_PATH]{};GetModuleFileNameW(nullptr,buf,MAX_PATH);return buf;}
static Brush ResolveTidyBrush(wchar_t const* key,Windows::UI::Color fallback,bool dark){
 try{return Application::Current().Resources().Lookup(box_value(key)).as<Brush>();}catch(...){}
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
Brush TidyWindow::ThemeBrush(wchar_t const* key,Windows::UI::Color fallback){bool dark=false;try{dark=root.ActualTheme()==ElementTheme::Dark;}catch(...){}return ResolveTidyBrush(key,fallback,dark);}
TidyWindow::TidyWindow(Controller& c):owner(c){
 window=Window();window.Title(i18n::Tr(L"GuoDesk 整理预览"));hwnd=shell::Handle(window);
 try{auto dir=std::filesystem::path(TidyExePath()).parent_path();window.AppWindow().SetIcon((dir/L"guodesk.ico").wstring());}catch(...){}
 window.SystemBackdrop(MakeBackdrop(owner.layout.settings.backdrop));
 root=Grid();root.Padding(Thickness{20,16,20,20});root.RowSpacing(10);
 RowDefinition head;head.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(head);
 RowDefinition body;body.Height(GridLength{1,GridUnitType::Star});root.RowDefinitions().Append(body);
 RowDefinition foot;foot.Height(GridLength{0,GridUnitType::Auto});root.RowDefinitions().Append(foot);
 StackPanel head2;head2.Orientation(Orientation::Vertical);head2.Spacing(2);
 TextBlock head3;head3.Text(i18n::Tr(L"整理桌面"));head3.FontSize(20);head3.FontWeight(Windows::UI::Text::FontWeights::SemiBold());head2.Children().Append(head3);
 summary=TextBlock();summary.FontSize(12);summary.TextWrapping(TextWrapping::Wrap);summary.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,97,97,97}));head2.Children().Append(summary);
 root.Children().Append(head2);
 auto scroll=ScrollViewer();scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
 list=StackPanel();list.Spacing(4);scroll.Content(list);Grid::SetRow(scroll,1);root.Children().Append(scroll);
 auto bar=StackPanel();bar.Orientation(Orientation::Horizontal);bar.HorizontalAlignment(HorizontalAlignment::Right);bar.Spacing(8);
 apply=Button();apply.Content(box_value(i18n::Tr(L"应用整理")));
 try{apply.Style(Application::Current().Resources().Lookup(box_value(L"AccentButtonStyle")).as<Style>());}catch(...){}
 apply.Click([this](auto&&,auto&&){if(applying||plan.empty())return;applying=true;int added=ApplyPlan(owner.layout,plan);owner.Save();owner.Refresh();apply.Content(box_value(added>0?i18n::TrF(L"已添加 {0} 个引用，即将关闭",{std::to_wstring(added)}):i18n::Tr(L"引用均已存在，即将关闭")));apply.IsEnabled(false);cancel.IsEnabled(false);closeTimer.Start();});
 bar.Children().Append(apply);
 cancel=Button();cancel.Content(box_value(i18n::Tr(L"关闭")));cancel.Click([this](auto&&,auto&&){owner.CloseTidy();});bar.Children().Append(cancel);
 Grid::SetRow(bar,2);root.Children().Append(bar);
 window.Content(root);
 closeTimer=root.DispatcherQueue().CreateTimer();closeTimer.Interval(std::chrono::milliseconds(1200));closeTimer.Tick([this](auto&&,auto&&){closeTimer.Stop();root.DispatcherQueue().TryEnqueue([this]{owner.CloseTidy();});});
 window.Closed([this](auto&&,auto&&){if(closing)return;closing=true;window.DispatcherQueue().TryEnqueue([this]{owner.CloseTidy();});});
 window.Activate();
}
void TidyWindow::Build(){
 plan.clear();unmatched.clear();auto files=DesktopFileList();
 plan=BuildPlan(owner.layout.rules,owner.layout.zones,files,&unmatched);
 list.Children().Clear();
 auto zoneName=[this](std::wstring const& id)->std::wstring{auto it=std::find_if(owner.layout.zones.begin(),owner.layout.zones.end(),[&](auto const& z){return z.id==id;});return it==owner.layout.zones.end()?L"":it->name;};
 for(auto const& p:plan){
  Border row;row.Padding(Thickness{10,8,10,8});row.CornerRadius(CornerRadius{6,6,6,6});row.Background(ThemeBrush(L"CardBackgroundFillColorDefault",Windows::UI::Color{255,60,60,60}));
  Grid g;ColumnDefinition c1;c1.Width(GridLength{1,GridUnitType::Star});g.ColumnDefinitions().Append(c1);ColumnDefinition c2;c2.Width(GridLength{0,GridUnitType::Auto});g.ColumnDefinitions().Append(c2);
  StackPanel left;left.Orientation(Orientation::Vertical);left.Spacing(1);
  TextBlock name;name.Text(std::filesystem::path(p.path).filename().wstring());name.FontSize(13);left.Children().Append(name);
  TextBlock path;path.Text(p.path);path.FontSize(11);path.TextTrimming(TextTrimming::CharacterEllipsis);path.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,160,160,160}));left.Children().Append(path);
  Grid::SetColumn(left,0);g.Children().Append(left);
  StackPanel right;right.Orientation(Orientation::Horizontal);right.Spacing(6);right.VerticalAlignment(VerticalAlignment::Center);
  FontIcon arrow;arrow.FontFamily(FontFamily(L"Segoe Fluent Icons"));arrow.Glyph(L"\uE72A");arrow.FontSize(12);right.Children().Append(arrow);
  Border tag;tag.Padding(Thickness{8,3,8,3});tag.CornerRadius(CornerRadius{10,10,10,10});tag.Background(ThemeBrush(L"SubtleFillColorSecondaryBrush",Windows::UI::Color{255,236,236,236}));
  TextBlock zone;zone.Text(zoneName(p.zone)+L" · "+p.rule);zone.FontSize(11);tag.Child(zone);right.Children().Append(tag);
  Grid::SetColumn(right,1);g.Children().Append(right);
  row.Child(g);list.Children().Append(row);
 }
 if(!unmatched.empty()){
  Border row;row.Padding(Thickness{10,8,10,8});row.CornerRadius(CornerRadius{6,6,6,6});
  TextBlock rest;rest.Text(i18n::TrF(L"另有 {0} 项未匹配规则，保持原位",{std::to_wstring(unmatched.size())}));rest.FontSize(12);rest.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,160,160,160}));row.Child(rest);list.Children().Append(row);
 }
 if(plan.empty()){apply.IsEnabled(false);apply.Content(box_value(i18n::Tr(L"没有匹配项")));}
 else{apply.IsEnabled(true);apply.Content(box_value(i18n::TrF(L"应用整理（添加 {0} 个引用）",{std::to_wstring(plan.size())})));}
 summary.Text(plan.empty()?i18n::Tr(L"桌面上没有匹配规则的新文件。"):i18n::TrF(L"以下 {0} 个桌面文件将作为引用加入分区，原文件保持原位。",{std::to_wstring(plan.size())}));
 window.Title(i18n::TrF(L"GuoDesk 整理预览 · {0} 项",{std::to_wstring(plan.size())}));
}
void TidyWindow::Show(){Build();window.Activate();}
TidyWindow::~TidyWindow(){closing=true;try{window.Closed(nullptr);}catch(...){}if(IsWindow(hwnd))window.Close();}
}
