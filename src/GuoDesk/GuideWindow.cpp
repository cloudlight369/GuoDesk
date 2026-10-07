#include "pch.h"
#include <algorithm>
#include "GuideWindow.h"
#include "DeskWindow.h"
#include "Shell.h"
#include "I18n.h"
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
namespace guodesk {
static std::wstring GuideExePath(){wchar_t buf[MAX_PATH]{};GetModuleFileNameW(nullptr,buf,MAX_PATH);return buf;}
static Brush GuideAccentBrush(){
 try{return Application::Current().Resources().Lookup(box_value(L"AccentFillColorDefaultBrush")).as<Brush>();}catch(...){}
 return SolidColorBrush(Windows::UI::Color{255,0,120,212});
}
static Brush GuideSecondaryBrush(){
 try{return Application::Current().Resources().Lookup(box_value(L"TextFillColorSecondaryBrush")).as<Brush>();}catch(...){}
 return SolidColorBrush(Windows::UI::Color{255,110,110,110});
}
GuideWindow::GuideWindow(Controller& c):owner(c){
 window=Window();window.Title(i18n::Tr(L"欢迎使用 GuoDesk"));hwnd=shell::Handle(window);
 try{auto dir=std::filesystem::path(GuideExePath()).parent_path();window.AppWindow().SetIcon((dir/L"guodesk.ico").wstring());}catch(...){}
 auto scroll=ScrollViewer();auto page=StackPanel();page.Padding(Thickness{32,28,32,24});page.Spacing(0);page.MaxWidth(480);
 TextBlock title;title.Text(i18n::Tr(L"欢迎使用 GuoDesk"));title.FontSize(22);title.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
 TextBlock sub;sub.Text(i18n::Tr(L"几分钟了解 GuoDesk 的核心功能。"));sub.FontSize(13);sub.Margin(Thickness{0,6,0,20});sub.Foreground(GuideSecondaryBrush());sub.TextWrapping(TextWrapping::Wrap);
 page.Children().Append(title);page.Children().Append(sub);
 struct Step{wchar_t const* name;wchar_t const* desc;};
 static const Step steps[]{
  {L"新增分区",L"托盘右键 →「新增分区」，把文件、文件夹或应用快捷方式直接拖进去，原文件保持原位。"},
  {L"整理桌面",L"托盘右键 →「整理桌面…」，按扩展名规则把散落文件归入分区，先预览再应用。"},
  {L"映射文件夹",L"分区右键 →「映射文件夹」，把任意文件夹实时镜像到桌面，只读展示。"},
  {L"便签与待办",L"托盘右键打开便签和待办；待办可设截止日期，到期时通过托盘气泡提醒。"},
  {L"时钟与热键",L"托盘右键打开时钟，右键时钟可查看日历；设置里可配置显示/隐藏的全局热键。"},
  {L"音乐·搜索·天气",L"托盘右键打开音乐播放器、全局搜索和天气组件，全部可在桌面自由摆放。"},
  {L"就地浏览",L"双击映射分区里的文件夹即可就地浏览，顶部面包屑一键返回。"},
  {L"快速捕获",L"随时从托盘或热键唤起捕获框：Enter 记入便签，Ctrl+Enter 存为待办。"},
  {L"个性化",L"设置里可选三档文字大小与数字/模拟两种时钟样式，改动立即生效。"},
  {L"从链接下载",L"分区右键 →「从链接下载…」，粘贴 http/https 链接就能下到该分区的映射文件夹，完成后自动选中那个文件。"},
  {L"点角标摊开这一叠",L"叠放磁贴右下角的数字可以点：在分区外面摊开这一叠，单击打开、Ctrl 单击多选、按住直接拖出去，分区高度一点不动。"},
 };
 for(int i=0;i<static_cast<int>(sizeof(steps)/sizeof(steps[0]));++i){
  auto row=Grid();row.Margin(Thickness{0,0,0,18});
  ColumnDefinition badgeCol;badgeCol.Width(GridLength{0,GridUnitType::Auto});row.ColumnDefinitions().Append(badgeCol);
  ColumnDefinition textCol;textCol.Width(GridLength{1,GridUnitType::Star});row.ColumnDefinitions().Append(textCol);
  auto badge=Border();badge.Width(30);badge.Height(30);badge.CornerRadius(CornerRadius{15,15,15,15});badge.Background(GuideAccentBrush());badge.VerticalAlignment(VerticalAlignment::Top);badge.HorizontalAlignment(HorizontalAlignment::Left);
  TextBlock num;num.Text(std::to_wstring(i+1));num.FontSize(13);num.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());num.Foreground(SolidColorBrush(Windows::UI::Colors::White()));num.HorizontalAlignment(HorizontalAlignment::Center);num.VerticalAlignment(VerticalAlignment::Center);
  badge.Child(num);Grid::SetColumn(badge,0);row.Children().Append(badge);
  auto texts=StackPanel();texts.Spacing(2);texts.Margin(Thickness{14,0,0,0});
  TextBlock name;name.Text(i18n::Tr(steps[i].name));name.FontSize(14);name.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
  TextBlock desc;desc.Text(i18n::Tr(steps[i].desc));desc.FontSize(12);desc.Foreground(GuideSecondaryBrush());desc.TextWrapping(TextWrapping::Wrap);
  texts.Children().Append(name);texts.Children().Append(desc);
  Grid::SetColumn(texts,1);row.Children().Append(texts);
  page.Children().Append(row);
 }
 auto start=Button();start.Content(box_value(i18n::Tr(L"开始使用")));try{start.Style(Application::Current().Resources().Lookup(box_value(L"AccentButtonStyle")).as<Style>());}catch(...){}
 start.HorizontalAlignment(HorizontalAlignment::Left);start.Margin(Thickness{0,8,0,0});
 start.Click([this](auto&&,auto&&){owner.layout.settings.guideDone=true;owner.Save();window.DispatcherQueue().TryEnqueue([this]{owner.CloseGuide();});});
 scroll.Content(page);scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
 // 「开始使用」钉在底部：条目一多就会被滚走，主操作不该要靠滚动才找得到
 Border bar;bar.Padding(Thickness{32,0,32,18});bar.Child(start);
 Grid host;
 {RowDefinition r0;r0.Height(GridLength{1,GridUnitType::Star});host.RowDefinitions().Append(r0);RowDefinition r1;r1.Height(GridLength{0,GridUnitType::Auto});host.RowDefinitions().Append(r1);
  Grid::SetRow(scroll,0);Grid::SetRow(bar,1);host.Children().Append(scroll);host.Children().Append(bar);}
 window.Content(host);
 window.Closed([this](auto&&,auto&&){if(closing)return;closing=true;window.DispatcherQueue().TryEnqueue([this]{owner.CloseGuide();});});
 window.Activate();
 // 高度跟着条目数长，但不超过工作区的八成，也不许小于能看清首屏的尺寸
 int const dpi=GetDpiForWindow(hwnd)?GetDpiForWindow(hwnd):96;
 MONITORINFOEXW mi{sizeof(mi)};
 if(!GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi))mi.rcWork=RECT{0,0,1280,1024};
 int const rows=static_cast<int>(sizeof(steps)/sizeof(steps[0]));
 int const ww=MulDiv(560,dpi,96),h=GuideWindowHeight(rows,dpi,mi.rcWork.bottom-mi.rcWork.top);
 // 位置跟着一起定：只改尺寸会原地往下长，工作区矮的时候「开始使用」正好长到屏幕外面
 SetWindowPos(hwnd,nullptr,mi.rcWork.left+((mi.rcWork.right-mi.rcWork.left)-ww)/2,mi.rcWork.top+((mi.rcWork.bottom-mi.rcWork.top)-h)/2,ww,h,SWP_NOZORDER);
}
void GuideWindow::Show(){window.Activate();}
GuideWindow::~GuideWindow(){closing=true;try{window.Closed(nullptr);}catch(...){}if(IsWindow(hwnd))window.Close();}
}
