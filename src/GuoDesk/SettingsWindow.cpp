#include "pch.h"
#include "SettingsWindow.h"
#include "DeskWindow.h"
#include "Shell.h"
#include "I18n.h"
#include "WeatherWindow.h"
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
namespace guodesk {
static std::wstring ExePath(){wchar_t buf[MAX_PATH]{};GetModuleFileNameW(nullptr,buf,MAX_PATH);return buf;}
static bool AutostartEnabled(){HKEY k{};if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_READ,&k)!=ERROR_SUCCESS)return false;DWORD type{},size{};LONG rc=RegQueryValueExW(k,L"GuoDesk",nullptr,&type,nullptr,&size);RegCloseKey(k);return rc==ERROR_SUCCESS&&(type==REG_SZ||type==REG_EXPAND_SZ)&&size>2;}
static void SetAutostart(bool on){HKEY k{};if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_SET_VALUE,&k)!=ERROR_SUCCESS){MessageBoxW(nullptr,i18n::Tr(L"无法写入注册表，请检查权限。").c_str(),L"GuoDesk",MB_OK|MB_ICONERROR);return;}if(on){auto quoted=L"\""+ExePath()+L"\"";RegSetValueExW(k,L"GuoDesk",0,REG_SZ,reinterpret_cast<BYTE const*>(quoted.c_str()),static_cast<DWORD>((quoted.size()+1)*sizeof(wchar_t)));}else RegDeleteValueW(k,L"GuoDesk");RegCloseKey(k);}
static TextBlock Caption(std::wstring const& text){TextBlock t;t.Text(text);t.FontSize(13);t.Margin(Thickness{0,14,0,6});t.Opacity(0.7);return t;}
static std::vector<std::wstring> SplitList(std::wstring const& value){std::vector<std::wstring> out;std::wstring cur;for(wchar_t ch:value){if(ch==L','||ch==L';'||ch==0xFF0C||ch==0xFF1B){if(!cur.empty())out.push_back(cur);cur.clear();}else if(!iswspace(ch))cur.push_back(ch);}if(!cur.empty())out.push_back(cur);for(auto& s:out){CharLowerBuffW(s.data(),static_cast<DWORD>(s.size()));if(!s.empty()&&s.front()==L'.')s.erase(s.begin());}return out;}
static winrt::Microsoft::UI::Xaml::Media::Brush CardBrush(){try{return Application::Current().Resources().Lookup(box_value(L"CardBackgroundFillColorDefault")).as<winrt::Microsoft::UI::Xaml::Media::Brush>();}catch(...){}return winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Color{255,248,248,248});}
SettingsWindow::SettingsWindow(Controller& c):owner(c){
 window=Window();window.Title(i18n::Tr(L"GuoDesk 设置"));hwnd=shell::Handle(window);
 try{auto dir=std::filesystem::path(ExePath()).parent_path();window.AppWindow().SetIcon((dir/L"guodesk.ico").wstring());}catch(...){}
 scroll=ScrollViewer();StackPanel panel;panel.Padding(Thickness{24,16,24,24});panel.MaxWidth(420);panel.HorizontalAlignment(HorizontalAlignment::Left);
 panel.Children().Append(Caption(i18n::Tr(L"外观")));
 theme=ComboBox();theme.HorizontalAlignment(HorizontalAlignment::Stretch);ComboBoxItem def;def.Content(box_value(i18n::Tr(L"跟随系统")));theme.Items().Append(def);ComboBoxItem light;light.Content(box_value(i18n::Tr(L"浅色")));theme.Items().Append(light);ComboBoxItem dark;dark.Content(box_value(i18n::Tr(L"深色")));theme.Items().Append(dark);
 theme.SelectionChanged([this](auto&&,auto&&){if(applying)return;OnTheme(theme.SelectedIndex());});panel.Children().Append(theme);
 compact=ToggleSwitch();compact.OnContent(box_value(i18n::Tr(L"紧凑磁贴")));compact.OffContent(box_value(i18n::Tr(L"紧凑磁贴")));compact.Toggled([this](auto&&,auto&&){if(applying)return;OnCompact(compact.IsOn());});panel.Children().Append(compact);
 panel.Children().Append(Caption(i18n::Tr(L"常规")));
 autostart=ToggleSwitch();autostart.OnContent(box_value(i18n::Tr(L"开机自动启动")));autostart.OffContent(box_value(i18n::Tr(L"开机自动启动")));autostart.Toggled([this](auto&&,auto&&){if(applying)return;OnAutostart(autostart.IsOn());});panel.Children().Append(autostart);
 panel.Children().Append(Caption(i18n::Tr(L"全局热键")));
 hotkey=ComboBox();hotkey.HorizontalAlignment(HorizontalAlignment::Stretch);for(wchar_t const* p:{L"Ctrl+Alt+G",L"Ctrl+Alt+Z",L"Ctrl+Shift+Space",L"Win+Z",L"禁用"}){ComboBoxItem it;it.Content(box_value(i18n::Tr(p)));hotkey.Items().Append(it);}
 hotkey.SelectionChanged([this](auto&&,auto&&){if(applying)return;OnHotkey(hotkey.SelectedIndex());});panel.Children().Append(hotkey);
 hotkeyHint=TextBlock();hotkeyHint.Text(i18n::Tr(L"热键已被其他程序占用，未生效。"));hotkeyHint.FontSize(11);hotkeyHint.Foreground(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Color{255,232,17,35}));hotkeyHint.Visibility(Visibility::Collapsed);panel.Children().Append(hotkeyHint);
 panel.Children().Append(Caption(i18n::Tr(L"搜索热键")));
 hotkeySearch=ComboBox();hotkeySearch.HorizontalAlignment(HorizontalAlignment::Stretch);for(wchar_t const* p:{L"禁用",L"Ctrl+Alt+F",L"Ctrl+Shift+F",L"Alt+Q"}){ComboBoxItem it;it.Content(box_value(i18n::Tr(p)));hotkeySearch.Items().Append(it);}
 hotkeySearch.SelectionChanged([this](auto&&,auto&&){if(applying)return;OnHotkeySearch(hotkeySearch.SelectedIndex());});panel.Children().Append(hotkeySearch);
 hotkeySearchHint=TextBlock();hotkeySearchHint.Text(i18n::Tr(L"热键已被其他程序占用，未生效。"));hotkeySearchHint.FontSize(11);hotkeySearchHint.Foreground(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Color{255,232,17,35}));hotkeySearchHint.Visibility(Visibility::Collapsed);panel.Children().Append(hotkeySearchHint);
 TextBlock searchHint;searchHint.Text(i18n::Tr(L"按热键随时唤起搜索框，快速打开分区里的内容。"));searchHint.FontSize(11);searchHint.Opacity(0.6);searchHint.TextWrapping(TextWrapping::Wrap);panel.Children().Append(searchHint);
 panel.Children().Append(Caption(i18n::Tr(L"天气")));
 StackPanel cityBar;cityBar.Orientation(Orientation::Horizontal);cityBar.Spacing(8);
 weatherCity=TextBox();weatherCity.PlaceholderText(i18n::Tr(L"城市名，如：上海"));weatherCity.Width(150);cityBar.Children().Append(weatherCity);
 auto geoBtn=Button();geoBtn.Content(box_value(i18n::Tr(L"搜索")));geoBtn.Click([this](auto&&,auto&&){OnGeoSearch();});cityBar.Children().Append(geoBtn);
 auto autoBtn=Button();autoBtn.Content(box_value(i18n::Tr(L"自动定位")));autoBtn.Click([this](auto&&,auto&&){OnGeoAuto();});cityBar.Children().Append(autoBtn);
 panel.Children().Append(cityBar);
 weatherResults=ComboBox();weatherResults.HorizontalAlignment(HorizontalAlignment::Stretch);weatherResults.Visibility(Visibility::Collapsed);weatherResults.Margin(Thickness{0,8,0,0});weatherResults.SelectionChanged([this](auto&&,auto&&){if(weatherSave)weatherSave.Visibility(weatherResults.SelectedIndex()>=0?Visibility::Visible:Visibility::Collapsed);});panel.Children().Append(weatherResults);
 weatherSave=Button();weatherSave.Content(box_value(i18n::Tr(L"使用该城市")));weatherSave.Margin(Thickness{0,8,0,0});weatherSave.Visibility(Visibility::Collapsed);weatherSave.Click([this](auto&&,auto&&){OnGeoSave();});panel.Children().Append(weatherSave);
 weatherHint=TextBlock();weatherHint.Text(i18n::Tr(L"默认自动定位，也可以手动选择城市。"));weatherHint.FontSize(11);weatherHint.Opacity(0.6);weatherHint.TextWrapping(TextWrapping::Wrap);weatherHint.Margin(Thickness{0,4,0,0});panel.Children().Append(weatherHint);
 panel.Children().Append(Caption(i18n::Tr(L"语言")));
 lang=ComboBox();lang.HorizontalAlignment(HorizontalAlignment::Stretch);ComboBoxItem langSys;langSys.Content(box_value(i18n::Tr(L"跟随系统")));lang.Items().Append(langSys);ComboBoxItem langZh;langZh.Content(box_value(L"简体中文"));lang.Items().Append(langZh);ComboBoxItem langEn;langEn.Content(box_value(L"English"));lang.Items().Append(langEn);
 lang.SelectionChanged([this](auto&&,auto&&){if(applying)return;OnLanguage(lang.SelectedIndex());});panel.Children().Append(lang);
 TextBlock langHint;langHint.Text(i18n::Tr(L"语言将在重启 GuoDesk 后生效。"));langHint.FontSize(11);langHint.Opacity(0.6);panel.Children().Append(langHint);
 panel.Children().Append(Caption(i18n::Tr(L"备份")));
 StackPanel backupBar;backupBar.Orientation(Orientation::Horizontal);backupBar.Spacing(8);
 auto exportBtn=Button();exportBtn.Content(box_value(i18n::Tr(L"导出配置")));exportBtn.Click([this](auto&&,auto&&){OnExport();});backupBar.Children().Append(exportBtn);
 auto importBtn=Button();importBtn.Content(box_value(i18n::Tr(L"导入配置")));importBtn.Click([this](auto&&,auto&&){OnImport();});backupBar.Children().Append(importBtn);
 panel.Children().Append(backupBar);
 snapshots=ToggleSwitch();snapshots.OnContent(box_value(i18n::Tr(L"配置自动快照")));snapshots.OffContent(box_value(i18n::Tr(L"配置自动快照")));snapshots.Toggled([this](auto&&,auto&&){if(applying)return;OnSnapshots(snapshots.IsOn());});panel.Children().Append(snapshots);
 TextBlock snapHint;snapHint.Text(i18n::Tr(L"每天首次及每 20 次保存各留一份，保留最近 5 份，配置损坏可自动恢复。"));snapHint.FontSize(11);snapHint.Opacity(0.6);snapHint.TextWrapping(TextWrapping::Wrap);panel.Children().Append(snapHint);
 auto guideLink=HyperlinkButton();guideLink.Content(box_value(i18n::Tr(L"查看新手引导")));guideLink.Margin(Thickness{0,2,0,0});guideLink.Padding(Thickness{0});guideLink.Click([this](auto&&,auto&&){owner.ShowGuide();});panel.Children().Append(guideLink);
 panel.Children().Append(Caption(i18n::Tr(L"整理规则")));
 TextBlock ruleIntro;ruleIntro.Text(i18n::Tr(L"按扩展名或文件名关键词，把桌面文件以引用方式归入分区——原文件始终保持在桌面。"));ruleIntro.FontSize(12);ruleIntro.TextWrapping(TextWrapping::Wrap);ruleIntro.Opacity(0.8);panel.Children().Append(ruleIntro);
 rulesPanel=StackPanel();rulesPanel.Spacing(4);rulesPanel.Padding(Thickness{0,6,0,0});panel.Children().Append(rulesPanel);
 StackPanel ruleBar;ruleBar.Orientation(Orientation::Horizontal);ruleBar.Spacing(8);ruleBar.Margin(Thickness{0,8,0,0});
 auto addRule=Button();addRule.Content(box_value(i18n::Tr(L"添加规则")));addRule.Click([this](auto&&,auto&&){EditRule(L"");});ruleBar.Children().Append(addRule);
 auto tidyBtn=Button();tidyBtn.Content(box_value(i18n::Tr(L"预览整理…")));tidyBtn.Click([this](auto&&,auto&&){owner.ShowTidy();});ruleBar.Children().Append(tidyBtn);
 panel.Children().Append(ruleBar);
 RebuildRules();
 panel.Children().Append(Caption(i18n::Tr(L"关于")));
 TextBlock about;about.Text(i18n::Tr(L"GuoDesk v1.0.0 · 桌面分区整理\n引用式入口：只存引用，不动原文件\n缺失入口可右键重新定位\n便签与待办：托盘右键开启，待办可设截止日期提醒\n时钟：托盘右键开启，右键时钟查看日历\n音乐·搜索·天气：托盘右键开启\n\nMIT License · cloudlight369"));about.FontSize(12);about.TextWrapping(TextWrapping::Wrap);about.Opacity(0.8);panel.Children().Append(about);
 scroll.Content(panel);window.Content(scroll);
 window.Closed([this](auto&&,auto&&){if(closing)return;closing=true;window.DispatcherQueue().TryEnqueue([this]{owner.CloseSettings();});});
 window.Activate();
}
void SettingsWindow::OnTheme(int index){auto& s=owner.layout.settings;s.theme=index==1?L"Light":index==2?L"Dark":L"";owner.ApplySettings();owner.Save();}
void SettingsWindow::OnCompact(bool on){owner.layout.settings.compact=on;owner.ApplySettings();owner.Save();}
void SettingsWindow::OnAutostart(bool on){SetAutostart(on);bool actual=AutostartEnabled();if(actual!=on){applying=true;autostart.IsOn(actual);applying=false;}}
void SettingsWindow::OnLanguage(int index){owner.layout.settings.language=index==1?L"zh-CN":index==2?L"en-US":L"";owner.Save();}
void SettingsWindow::OnHotkey(int index){auto& s=owner.layout.settings;s.hotkey=index==1?L"Ctrl+Alt+Z":index==2?L"Ctrl+Shift+Space":index==3?L"Win+Z":index==4?L"":L"Ctrl+Alt+G";bool ok=owner.ApplyHotkey();hotkeyHint.Visibility(ok?Visibility::Collapsed:Visibility::Visible);owner.Save();}
void SettingsWindow::OnHotkeySearch(int index){auto& s=owner.layout.settings;s.hotkeySearch=index==1?L"Ctrl+Alt+F":index==2?L"Ctrl+Shift+F":index==3?L"Alt+Q":L"";bool ok=owner.ApplyHotkey();hotkeySearchHint.Visibility(ok?Visibility::Collapsed:Visibility::Visible);owner.Save();}
void SettingsWindow::OnGeoSearch(){auto name=std::wstring(weatherCity.Text());size_t a=name.find_first_not_of(L" \t");size_t b=name.find_last_not_of(L" \t");name=a==std::wstring::npos?L"":name.substr(a,b-a+1);if(name.empty())return;weatherHint.Text(i18n::Tr(L"正在搜索…"));weatherResults.Visibility(Visibility::Collapsed);weatherSave.Visibility(Visibility::Collapsed);auto weak=std::weak_ptr<bool>(alive);std::thread([this,weak,name]{auto json=HttpGetJson(L"geocoding-api.open-meteo.com",L"/v1/search?count=5&language=zh-CN&format=json&name="+UrlParam(name),false);auto list=ParseGeoJson(std::wstring(winrt::to_hstring(json)));window.DispatcherQueue().TryEnqueue([this,weak,list]{if(weak.lock()==nullptr||closing||!weatherResults)return;geo=list;weatherResults.Items().Clear();for(auto const& g:list)weatherResults.Items().Append(box_value(g.country.empty()?g.name:g.name+L" · "+g.country));weatherResults.Visibility(Visibility::Visible);weatherHint.Text(list.empty()?i18n::Tr(L"没有找到这个城市，换个名字试试。"):i18n::Tr(L"选择一个城市后点击“使用该城市”。"));weatherSave.Visibility(Visibility::Collapsed);});}).detach();}
void SettingsWindow::OnGeoSave(){int sel=weatherResults.SelectedIndex();if(sel<0||sel>=static_cast<int>(geo.size()))return;auto& s=owner.layout.widgets;s.weatherCity=geo[static_cast<size_t>(sel)].name;s.weatherLat=geo[static_cast<size_t>(sel)].lat;s.weatherLon=geo[static_cast<size_t>(sel)].lon;owner.Save();weatherHint.Text(i18n::Tr(L"已保存，天气组件将使用所选城市。"));weatherResults.Visibility(Visibility::Collapsed);weatherSave.Visibility(Visibility::Collapsed);if(owner.weather)owner.weather->Reload();}
void SettingsWindow::OnGeoAuto(){auto& s=owner.layout.widgets;s.weatherCity.clear();s.weatherLat=999;s.weatherLon=999;owner.Save();weatherCity.Text(L"");weatherHint.Text(i18n::Tr(L"已恢复自动定位。"));weatherResults.Visibility(Visibility::Collapsed);weatherSave.Visibility(Visibility::Collapsed);if(owner.weather)owner.weather->Reload();}
void SettingsWindow::OnSnapshots(bool on){owner.layout.settings.snapshots=on;owner.store.SetSnapshots(on);owner.Save();}
void SettingsWindow::Apply(){applying=true;auto const& s=owner.layout.settings;theme.SelectedIndex(s.theme==L"Light"?1:s.theme==L"Dark"?2:0);compact.IsOn(s.compact);autostart.IsOn(AutostartEnabled());lang.SelectedIndex(s.language==L"en-US"?2:s.language==L"zh-CN"?1:0);hotkey.SelectedIndex(s.hotkey==L"Ctrl+Alt+Z"?1:s.hotkey==L"Ctrl+Shift+Space"?2:s.hotkey==L"Win+Z"?3:s.hotkey.empty()?4:0);hotkeySearch.SelectedIndex(s.hotkeySearch==L"Ctrl+Alt+F"?1:s.hotkeySearch==L"Ctrl+Shift+F"?2:s.hotkeySearch==L"Alt+Q"?3:0);weatherCity.Text(owner.layout.widgets.weatherCity);snapshots.IsOn(s.snapshots);hotkeyHint.Visibility(Visibility::Collapsed);hotkeySearchHint.Visibility(Visibility::Collapsed);applying=false;}
static std::string ReadTextFile(std::filesystem::path const& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Cannot read file");return {std::istreambuf_iterator<char>(f),{}};}
void SettingsWindow::OnExport(){owner.Save();auto target=shell::SaveFile(hwnd,L"guodesk-layout.json");if(target.empty())return;try{std::filesystem::copy_file(owner.store.Directory()/L"layout.json",target,std::filesystem::copy_options::overwrite_existing);}catch(...){MessageBoxW(hwnd,i18n::Tr(L"导出失败：请检查目标位置是否可写。").c_str(),L"GuoDesk",MB_OK|MB_ICONERROR);return;}if(!owner.windows.empty())owner.windows.front()->Notify(i18n::TrF(L"已导出到 {0}",{target}));}
void SettingsWindow::OnImport(){auto picked=shell::Pick(hwnd,false,i18n::Tr(L"选择要导入的 GuoDesk 配置"));if(picked.size()!=1)return;std::string text;try{text=ReadTextFile(std::filesystem::path(picked[0]));}catch(...){MessageBoxW(hwnd,i18n::Tr(L"导入失败：文件不是有效的 GuoDesk 配置。").c_str(),L"GuoDesk",MB_OK|MB_ICONERROR);return;}Layout next;try{next=Deserialize(text);}catch(...){MessageBoxW(hwnd,i18n::Tr(L"导入失败：文件不是有效的 GuoDesk 配置。").c_str(),L"GuoDesk",MB_OK|MB_ICONERROR);return;}owner.ImportLayout(std::move(next));Controller* c=&owner;winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread().TryEnqueue([c](){c->CloseSettings();});}
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
  std::wstring match;for(size_t i=0;i<r.exts.size();++i){if(i)match+=L" / ";match+=r.exts[i];}if(!r.keywords.empty()){if(!match.empty())match+=i18n::Tr(L" · 关键词 ");for(size_t i=0;i<r.keywords.size();++i){if(i)match+=L" ";match+=r.keywords[i];}}
  TextBlock matchText;matchText.Text(match.empty()?i18n::Tr(L"未设置匹配条件"):match);matchText.FontSize(11);matchText.Opacity(0.75);left.Children().Append(matchText);
  TextBlock dest;dest.Text(r.targetZone.empty()?i18n::Tr(L"未绑定分区"):(L"→ "+zoneName(r.targetZone)));dest.FontSize(11);dest.Opacity(0.75);left.Children().Append(dest);
  open.Content(left);open.Click([this,key](auto&&,auto&&){EditRule(key);});Grid::SetColumn(open,0);g.Children().Append(open);
  FontIcon trashIcon;trashIcon.FontFamily(FontFamily(L"Segoe Fluent Icons"));trashIcon.Glyph(L"\uE74D");trashIcon.FontSize(14);
  auto del=Button();del.Content(trashIcon);del.Background(nullptr);del.BorderThickness(Thickness{0});del.VerticalAlignment(VerticalAlignment::Center);ToolTipService::SetToolTip(del,box_value(i18n::Tr(L"删除规则")));
  del.Click([this,key](auto&&,auto&&){auto& rs=owner.layout.rules;std::erase_if(rs,[&](auto const& x){return x.id==key;});owner.Save();RebuildRules();});
  Grid::SetColumn(del,1);g.Children().Append(del);
  row.Child(g);rulesPanel.Children().Append(row);
 }
 if(owner.layout.rules.empty()){TextBlock none;none.Text(i18n::Tr(L"暂无规则，点击下方“添加规则”。"));none.FontSize(12);none.Opacity(0.6);rulesPanel.Children().Append(none);}
}
void SettingsWindow::EditRule(std::wstring ruleId){
 auto& rules=owner.layout.rules;
 bool isNew=ruleId.empty();
 ContentDialog dlg;dlg.Title(box_value(isNew?i18n::Tr(L"添加规则"):i18n::Tr(L"编辑规则")));dlg.PrimaryButtonText(i18n::Tr(L"保存"));dlg.CloseButtonText(i18n::Tr(L"取消"));dlg.DefaultButton(ContentDialogButton::Primary);
 StackPanel p;p.Spacing(6);p.Padding(Thickness{0,8,0,4});p.MaxWidth(360);
 auto label=[](std::wstring const& t){TextBlock b;b.Text(t);b.FontSize(12);b.Opacity(0.7);return b;};
 TextBox nameBox;nameBox.PlaceholderText(i18n::Tr(L"如：文档"));if(!isNew)for(auto const& r:rules)if(r.id==ruleId)nameBox.Text(r.name);
 p.Children().Append(label(i18n::Tr(L"规则名称")));p.Children().Append(nameBox);
 TextBox extBox;extBox.PlaceholderText(i18n::Tr(L"doc, pdf, txt（逗号分隔，可留空）"));if(!isNew){std::wstring v;for(auto const& r:rules)if(r.id==ruleId){for(size_t i=0;i<r.exts.size();++i){if(i)v+=L", ";v+=r.exts[i];}}extBox.Text(v);}
 p.Children().Append(label(i18n::Tr(L"按扩展名匹配")));p.Children().Append(extBox);
 TextBox keyBox;keyBox.PlaceholderText(i18n::Tr(L"如：简历, 报告（文件名包含即可）"));if(!isNew){std::wstring v;for(auto const& r:rules)if(r.id==ruleId){for(size_t i=0;i<r.keywords.size();++i){if(i)v+=L", ";v+=r.keywords[i];}}keyBox.Text(v);}
 p.Children().Append(label(i18n::Tr(L"按文件名关键词匹配")));p.Children().Append(keyBox);
 ComboBox zoneBox;zoneBox.HorizontalAlignment(HorizontalAlignment::Stretch);zoneBox.Items().Append(box_value(i18n::Tr(L"（未绑定）")));
 int defIndex=0,index=1;for(auto const& z:owner.layout.zones){zoneBox.Items().Append(box_value(z.name));if(!isNew)for(auto const& r:rules)if(r.id==ruleId&&r.targetZone==z.id)defIndex=index;++index;}
 zoneBox.SelectedIndex(defIndex);p.Children().Append(label(i18n::Tr(L"整理到分区")));p.Children().Append(zoneBox);
 dlg.Content(p);
 try{dlg.XamlRoot(scroll.XamlRoot());}catch(...){return;}
 auto op=dlg.ShowAsync();
 op.Completed([this,ruleId,isNew,nameBox,extBox,keyBox,zoneBox](auto&& async,auto&&){
  if(async.GetResults()!=ContentDialogResult::Primary)return;
  auto& rs=owner.layout.rules;
  Rule* t=nullptr;for(auto& r:rs)if(r.id==ruleId)t=&r;
  if(!t){rs.push_back({});t=&rs.back();t->id=ruleId.empty()?NewId():ruleId;t->name=i18n::Tr(L"新规则");}
  t->name=std::wstring(nameBox.Text());if(t->name.empty())t->name=i18n::Tr(L"未命名规则");
  t->exts=SplitList(std::wstring(extBox.Text()));
  t->keywords=SplitList(std::wstring(keyBox.Text()));
  int sel=zoneBox.SelectedIndex();t->targetZone=(sel>0&&sel<=static_cast<int>(owner.layout.zones.size()))?owner.layout.zones[static_cast<size_t>(sel-1)].id:L"";
  owner.Save();RebuildRules();
 });
}
}
