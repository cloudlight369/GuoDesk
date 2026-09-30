#include "pch.h"
#include "SettingsWindow.h"
#include "DeskWindow.h"
#include "Shell.h"
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
namespace guodesk {
static std::wstring ExePath(){wchar_t buf[MAX_PATH]{};GetModuleFileNameW(nullptr,buf,MAX_PATH);return buf;}
static bool AutostartEnabled(){HKEY k{};if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_READ,&k)!=ERROR_SUCCESS)return false;DWORD type{},size{};LONG rc=RegQueryValueExW(k,L"GuoDesk",nullptr,&type,nullptr,&size);RegCloseKey(k);return rc==ERROR_SUCCESS&&(type==REG_SZ||type==REG_EXPAND_SZ)&&size>2;}
static void SetAutostart(bool on){HKEY k{};if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_SET_VALUE,&k)!=ERROR_SUCCESS){MessageBoxW(nullptr,L"无法写入注册表，请检查权限。",L"GuoDesk",MB_OK|MB_ICONERROR);return;}if(on){auto quoted=L"\""+ExePath()+L"\"";RegSetValueExW(k,L"GuoDesk",0,REG_SZ,reinterpret_cast<BYTE const*>(quoted.c_str()),static_cast<DWORD>((quoted.size()+1)*sizeof(wchar_t)));}else RegDeleteValueW(k,L"GuoDesk");RegCloseKey(k);}
static TextBlock Caption(wchar_t const* text){TextBlock t;t.Text(text);t.FontSize(13);t.Margin(Thickness{0,14,0,6});t.Opacity(0.7);return t;}
static std::vector<std::wstring> SplitList(std::wstring const& value){std::vector<std::wstring> out;std::wstring cur;for(wchar_t ch:value){if(ch==L','||ch==L';'||ch==0xFF0C||ch==0xFF1B){if(!cur.empty())out.push_back(cur);cur.clear();}else if(!iswspace(ch))cur.push_back(ch);}if(!cur.empty())out.push_back(cur);for(auto& s:out){CharLowerBuffW(s.data(),static_cast<DWORD>(s.size()));if(!s.empty()&&s.front()==L'.')s.erase(s.begin());}return out;}
static winrt::Microsoft::UI::Xaml::Media::Brush CardBrush(){try{return Application::Current().Resources().Lookup(box_value(L"CardBackgroundFillColorDefault")).as<winrt::Microsoft::UI::Xaml::Media::Brush>();}catch(...){}return winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Color{255,248,248,248});}
SettingsWindow::SettingsWindow(Controller& c):owner(c){
 window=Window();window.Title(L"GuoDesk 设置");hwnd=shell::Handle(window);
 try{auto dir=std::filesystem::path(ExePath()).parent_path();window.AppWindow().SetIcon((dir/L"guodesk.ico").wstring());}catch(...){}
 scroll=ScrollViewer();StackPanel panel;panel.Padding(Thickness{24,16,24,24});panel.MaxWidth(420);panel.HorizontalAlignment(HorizontalAlignment::Left);
 panel.Children().Append(Caption(L"外观"));
 theme=ComboBox();theme.HorizontalAlignment(HorizontalAlignment::Stretch);ComboBoxItem def;def.Content(box_value(L"跟随系统"));theme.Items().Append(def);ComboBoxItem light;light.Content(box_value(L"浅色"));theme.Items().Append(light);ComboBoxItem dark;dark.Content(box_value(L"深色"));theme.Items().Append(dark);
 theme.SelectionChanged([this](auto&&,auto&&){if(applying)return;OnTheme(theme.SelectedIndex());});panel.Children().Append(theme);
 compact=ToggleSwitch();compact.OnContent(box_value(L"紧凑磁贴"));compact.OffContent(box_value(L"紧凑磁贴"));compact.Toggled([this](auto&&,auto&&){if(applying)return;OnCompact(compact.IsOn());});panel.Children().Append(compact);
 panel.Children().Append(Caption(L"常规"));
 autostart=ToggleSwitch();autostart.OnContent(box_value(L"开机自动启动"));autostart.OffContent(box_value(L"开机自动启动"));autostart.Toggled([this](auto&&,auto&&){if(applying)return;OnAutostart(autostart.IsOn());});panel.Children().Append(autostart);
 panel.Children().Append(Caption(L"整理规则"));
 TextBlock ruleIntro;ruleIntro.Text(L"按扩展名或文件名关键词，把桌面文件以引用方式归入分区——原文件始终保持在桌面。");ruleIntro.FontSize(12);ruleIntro.TextWrapping(TextWrapping::Wrap);ruleIntro.Opacity(0.8);panel.Children().Append(ruleIntro);
 rulesPanel=StackPanel();rulesPanel.Spacing(4);rulesPanel.Padding(Thickness{0,6,0,0});panel.Children().Append(rulesPanel);
 StackPanel ruleBar;ruleBar.Orientation(Orientation::Horizontal);ruleBar.Spacing(8);ruleBar.Margin(Thickness{0,8,0,0});
 auto addRule=Button();addRule.Content(box_value(L"添加规则"));addRule.Click([this](auto&&,auto&&){EditRule(L"");});ruleBar.Children().Append(addRule);
 auto tidyBtn=Button();tidyBtn.Content(box_value(L"预览整理…"));tidyBtn.Click([this](auto&&,auto&&){owner.ShowTidy();});ruleBar.Children().Append(tidyBtn);
 panel.Children().Append(ruleBar);
 RebuildRules();
 panel.Children().Append(Caption(L"关于"));
 TextBlock about;about.Text(L"GuoDesk v0.5.0 · 桌面分区整理\n引用式入口：只存引用，不动原文件\n缺失入口可右键重新定位\n便签与待办：托盘右键开启，本地保存\n\nMIT License · cloudlight369");about.FontSize(12);about.TextWrapping(TextWrapping::Wrap);about.Opacity(0.8);panel.Children().Append(about);
 scroll.Content(panel);window.Content(scroll);
 window.Closed([this](auto&&,auto&&){if(closing)return;closing=true;window.DispatcherQueue().TryEnqueue([this]{owner.CloseSettings();});});
 window.Activate();
}
void SettingsWindow::OnTheme(int index){auto& s=owner.layout.settings;s.theme=index==1?L"Light":index==2?L"Dark":L"";owner.ApplySettings();owner.Save();}
void SettingsWindow::OnCompact(bool on){owner.layout.settings.compact=on;owner.ApplySettings();owner.Save();}
void SettingsWindow::OnAutostart(bool on){SetAutostart(on);bool actual=AutostartEnabled();if(actual!=on){applying=true;autostart.IsOn(actual);applying=false;}}
void SettingsWindow::Apply(){applying=true;auto const& s=owner.layout.settings;theme.SelectedIndex(s.theme==L"Light"?1:s.theme==L"Dark"?2:0);compact.IsOn(s.compact);autostart.IsOn(AutostartEnabled());applying=false;}
void SettingsWindow::Show(){window.Activate();}
SettingsWindow::~SettingsWindow(){closing=true;if(IsWindow(hwnd))window.Close();}
void SettingsWindow::RebuildRules(){
 rulesPanel.Children().Clear();
 auto zoneName=[this](std::wstring const& id)->std::wstring{auto it=std::find_if(owner.layout.zones.begin(),owner.layout.zones.end(),[&](auto const& z){return z.id==id;});return it==owner.layout.zones.end()?L"":it->name;};
 for(auto const& r:owner.layout.rules){
  auto key=r.id;
  Border row;row.Padding(Thickness{10,8,10,8});row.CornerRadius(CornerRadius{6,6,6,6});row.Background(CardBrush());
  Grid g;ColumnDefinition c1;c1.Width(GridLength{1,GridUnitType::Star});g.ColumnDefinitions().Append(c1);ColumnDefinition c2;c2.Width(GridLength{0,GridUnitType::Auto});g.ColumnDefinitions().Append(c2);
  auto open=Button();open.Background(nullptr);open.BorderThickness(Thickness{0});open.HorizontalAlignment(HorizontalAlignment::Stretch);open.HorizontalContentAlignment(HorizontalAlignment::Left);open.Padding(Thickness{0});
  StackPanel left;left.Orientation(Orientation::Vertical);left.Spacing(1);
  TextBlock name;name.Text(r.name);name.FontSize(13);name.FontWeight(Windows::UI::Text::FontWeights::SemiBold());left.Children().Append(name);
  std::wstring match;for(size_t i=0;i<r.exts.size();++i){if(i)match+=L" / ";match+=r.exts[i];}if(!r.keywords.empty()){if(!match.empty())match+=L" · 关键词 ";for(size_t i=0;i<r.keywords.size();++i){if(i)match+=L" ";match+=r.keywords[i];}}
  TextBlock matchText;matchText.Text(match.empty()?L"未设置匹配条件":match);matchText.FontSize(11);matchText.Opacity(0.75);left.Children().Append(matchText);
  TextBlock dest;dest.Text(r.targetZone.empty()?L"未绑定分区":(L"→ "+zoneName(r.targetZone)));dest.FontSize(11);dest.Opacity(0.75);left.Children().Append(dest);
  open.Content(left);open.Click([this,key](auto&&,auto&&){EditRule(key);});Grid::SetColumn(open,0);g.Children().Append(open);
  FontIcon trashIcon;trashIcon.FontFamily(FontFamily(L"Segoe Fluent Icons"));trashIcon.Glyph(L"\uE74D");trashIcon.FontSize(14);
  auto del=Button();del.Content(trashIcon);del.Background(nullptr);del.BorderThickness(Thickness{0});del.VerticalAlignment(VerticalAlignment::Center);ToolTipService::SetToolTip(del,box_value(L"删除规则"));
  del.Click([this,key](auto&&,auto&&){auto& rs=owner.layout.rules;std::erase_if(rs,[&](auto const& x){return x.id==key;});owner.Save();RebuildRules();});
  Grid::SetColumn(del,1);g.Children().Append(del);
  row.Child(g);rulesPanel.Children().Append(row);
 }
 if(owner.layout.rules.empty()){TextBlock none;none.Text(L"暂无规则，点击下方“添加规则”。");none.FontSize(12);none.Opacity(0.6);rulesPanel.Children().Append(none);}
}
void SettingsWindow::EditRule(std::wstring ruleId){
 auto& rules=owner.layout.rules;
 bool isNew=ruleId.empty();
 ContentDialog dlg;dlg.Title(box_value(isNew?L"添加规则":L"编辑规则"));dlg.PrimaryButtonText(L"保存");dlg.CloseButtonText(L"取消");dlg.DefaultButton(ContentDialogButton::Primary);
 StackPanel p;p.Spacing(6);p.Padding(Thickness{0,8,0,4});p.MaxWidth(360);
 auto label=[](wchar_t const* t){TextBlock b;b.Text(t);b.FontSize(12);b.Opacity(0.7);return b;};
 TextBox nameBox;nameBox.PlaceholderText(L"如：文档");if(!isNew)for(auto const& r:rules)if(r.id==ruleId)nameBox.Text(r.name);
 p.Children().Append(label(L"规则名称"));p.Children().Append(nameBox);
 TextBox extBox;extBox.PlaceholderText(L"doc, pdf, txt（逗号分隔，可留空）");if(!isNew){std::wstring v;for(auto const& r:rules)if(r.id==ruleId){for(size_t i=0;i<r.exts.size();++i){if(i)v+=L", ";v+=r.exts[i];}}extBox.Text(v);}
 p.Children().Append(label(L"按扩展名匹配"));p.Children().Append(extBox);
 TextBox keyBox;keyBox.PlaceholderText(L"如：简历, 报告（文件名包含即可）");if(!isNew){std::wstring v;for(auto const& r:rules)if(r.id==ruleId){for(size_t i=0;i<r.keywords.size();++i){if(i)v+=L", ";v+=r.keywords[i];}}keyBox.Text(v);}
 p.Children().Append(label(L"按文件名关键词匹配"));p.Children().Append(keyBox);
 ComboBox zoneBox;zoneBox.HorizontalAlignment(HorizontalAlignment::Stretch);zoneBox.Items().Append(box_value(L"（未绑定）"));
 int defIndex=0,index=1;for(auto const& z:owner.layout.zones){zoneBox.Items().Append(box_value(z.name));if(!isNew)for(auto const& r:rules)if(r.id==ruleId&&r.targetZone==z.id)defIndex=index;++index;}
 zoneBox.SelectedIndex(defIndex);p.Children().Append(label(L"整理到分区"));p.Children().Append(zoneBox);
 dlg.Content(p);
 try{dlg.XamlRoot(scroll.XamlRoot());}catch(...){return;}
 auto op=dlg.ShowAsync();
 op.Completed([this,ruleId,isNew,nameBox,extBox,keyBox,zoneBox](auto&& async,auto&&){
  if(async.GetResults()!=ContentDialogResult::Primary)return;
  auto& rs=owner.layout.rules;
  Rule* t=nullptr;for(auto& r:rs)if(r.id==ruleId)t=&r;
  if(!t){rs.push_back({});t=&rs.back();t->id=ruleId.empty()?NewId():ruleId;t->name=L"新规则";}
  t->name=std::wstring(nameBox.Text());if(t->name.empty())t->name=L"未命名规则";
  t->exts=SplitList(std::wstring(extBox.Text()));
  t->keywords=SplitList(std::wstring(keyBox.Text()));
  int sel=zoneBox.SelectedIndex();t->targetZone=(sel>0&&sel<=static_cast<int>(owner.layout.zones.size()))?owner.layout.zones[static_cast<size_t>(sel-1)].id:L"";
  owner.Save();RebuildRules();
 });
}
}
