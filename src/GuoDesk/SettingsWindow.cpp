#include "pch.h"
#include "SettingsWindow.h"
#include "DeskWindow.h"
#include "Shell.h"
#include "I18n.h"
#include "WeatherWindow.h"
#include "WebDav.h"
#include "EvSearch.h"
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
namespace guodesk {
static std::wstring ExePath(){wchar_t buf[MAX_PATH]{};GetModuleFileNameW(nullptr,buf,MAX_PATH);return buf;}
static std::wstring DiagVersion();
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
 performance=ToggleSwitch();performance.OnContent(box_value(i18n::Tr(L"性能模式")));performance.OffContent(box_value(i18n::Tr(L"性能模式")));performance.Toggled([this](auto&&,auto&&){if(applying)return;OnPerformance(performance.IsOn());});panel.Children().Append(performance);
 TextBlock perfHint;perfHint.Text(i18n::Tr(L"关闭背景效果与入场动画，低配电脑上更流畅。"));perfHint.FontSize(11);perfHint.Opacity(0.6);perfHint.TextWrapping(TextWrapping::Wrap);panel.Children().Append(perfHint);
 panel.Children().Append(Caption(i18n::Tr(L"文字大小")));
 textSize=ComboBox();textSize.HorizontalAlignment(HorizontalAlignment::Stretch);for(wchar_t const* p:{L"小",L"标准",L"大"}){ComboBoxItem it;it.Content(box_value(i18n::Tr(p)));textSize.Items().Append(it);}
 textSize.SelectionChanged([this](auto&&,auto&&){if(applying)return;OnTextSize(textSize.SelectedIndex());});panel.Children().Append(textSize);
 TextBlock sizeHint;sizeHint.Text(i18n::Tr(L"调整分区与组件的文字大小，立即生效。"));sizeHint.FontSize(11);sizeHint.Opacity(0.6);sizeHint.TextWrapping(TextWrapping::Wrap);panel.Children().Append(sizeHint);
 clockStyle=ComboBox();clockStyle.HorizontalAlignment(HorizontalAlignment::Stretch);for(wchar_t const* p:{L"数字时钟",L"模拟表盘"}){ComboBoxItem it;it.Content(box_value(i18n::Tr(p)));clockStyle.Items().Append(it);}
 clockStyle.SelectionChanged([this](auto&&,auto&&){if(applying)return;OnClockStyle(clockStyle.SelectedIndex());});panel.Children().Append(clockStyle);
 TextBlock clockHint;clockHint.Text(i18n::Tr(L"时钟组件的显示样式。"));clockHint.FontSize(11);clockHint.Opacity(0.6);panel.Children().Append(clockHint);
 backdrop=ComboBox();backdrop.HorizontalAlignment(HorizontalAlignment::Stretch);for(wchar_t const* p:{L"云母",L"亚克力"}){ComboBoxItem it;it.Content(box_value(i18n::Tr(p)));backdrop.Items().Append(it);}
 backdrop.SelectionChanged([this](auto&&,auto&&){if(applying)return;OnBackdrop(backdrop.SelectedIndex());});panel.Children().Append(backdrop);
 TextBlock bdHint;bdHint.Text(i18n::Tr(L"窗口背景材质，立即生效。"));bdHint.FontSize(11);bdHint.Opacity(0.6);bdHint.TextWrapping(TextWrapping::Wrap);panel.Children().Append(bdHint);
 panel.Children().Append(Caption(i18n::Tr(L"常规")));
 autostart=ToggleSwitch();autostart.OnContent(box_value(i18n::Tr(L"开机自动启动")));autostart.OffContent(box_value(i18n::Tr(L"开机自动启动")));autostart.Toggled([this](auto&&,auto&&){if(applying)return;OnAutostart(autostart.IsOn());});panel.Children().Append(autostart);
 everythingToggle=ToggleSwitch();everythingToggle.OnContent(box_value(i18n::Tr(L"Everything 本地文件搜索")));everythingToggle.OffContent(box_value(i18n::Tr(L"Everything 本地文件搜索")));everythingToggle.Toggled([this](auto&&,auto&&){if(applying)return;OnEverything(everythingToggle.IsOn());});panel.Children().Append(everythingToggle);
 evHint=TextBlock();evHint.FontSize(11);evHint.Opacity(0.6);evHint.TextWrapping(TextWrapping::Wrap);panel.Children().Append(evHint);
 panel.Children().Append(Caption(i18n::Tr(L"全局热键")));
 hotkey=ComboBox();hotkey.HorizontalAlignment(HorizontalAlignment::Stretch);for(wchar_t const* p:{L"双击 Ctrl",L"Ctrl+Alt+G",L"Ctrl+Alt+Z",L"Ctrl+Shift+Space",L"Win+Z",L"自定义…",L"禁用"}){ComboBoxItem it;it.Content(box_value(i18n::Tr(p)));hotkey.Items().Append(it);}
 hotkey.SelectionChanged([this](auto&&,auto&&){if(applying)return;OnHotkey(hotkey.SelectedIndex());});panel.Children().Append(hotkey);
 hotkeyCustom=TextBox();hotkeyCustom.HorizontalAlignment(HorizontalAlignment::Stretch);hotkeyCustom.Margin(Thickness{0,6,0,0});hotkeyCustom.PlaceholderText(i18n::Tr(L"点击此处，然后按下热键组合"));hotkeyCustom.IsReadOnly(true);panel.Children().Append(hotkeyCustom);
 hotkeyCustom.KeyDown([this](auto&&,Input::KeyRoutedEventArgs const& a){
  auto key=a.Key();a.Handled(true);
  UINT vk=static_cast<UINT>(key);
  if(vk==VK_CONTROL||vk==VK_SHIFT||vk==VK_MENU||vk==VK_LWIN||vk==VK_RWIN){hotkeyCustom.Text(i18n::Tr(L"继续，按下组合键中的主键…"));return;}
  Hotkey hk{};
  if(GetKeyState(VK_CONTROL)&0x8000)hk.mods|=MOD_CONTROL;
  if(GetKeyState(VK_MENU)&0x8000)hk.mods|=MOD_ALT;
  if(GetKeyState(VK_SHIFT)&0x8000)hk.mods|=MOD_SHIFT;
  if((GetKeyState(VK_LWIN)|GetKeyState(VK_RWIN))&0x8000)hk.mods|=MOD_WIN;
  hk.vk=vk;
  std::wstring text=HotkeyToString(hk);
  Hotkey check{};
  if(text.empty()||!ParseHotkey(text,check)){hotkeyHint.Text(i18n::Tr(L"组合键需包含 Ctrl、Alt 或 Win 修饰键。"));hotkeyHint.Visibility(Visibility::Visible);return;}
  auto& s=owner.layout.settings;
  s.hotkey=text;
  hotkeyCustom.Text(text);
  applying=true;hotkey.SelectedIndex(5);applying=false;
  bool ok=owner.ApplyHotkey();
  hotkeyHint.Text(i18n::Tr(L"热键已被其他程序占用，未生效。"));
  hotkeyHint.Visibility(ok?Visibility::Collapsed:Visibility::Visible);
  owner.Save();
 });
 hotkeyHint=TextBlock();hotkeyHint.Text(i18n::Tr(L"热键已被其他程序占用，未生效。"));hotkeyHint.FontSize(11);hotkeyHint.Foreground(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Color{255,232,17,35}));hotkeyHint.Visibility(Visibility::Collapsed);panel.Children().Append(hotkeyHint);
 panel.Children().Append(Caption(i18n::Tr(L"搜索热键")));
 hotkeySearch=ComboBox();hotkeySearch.HorizontalAlignment(HorizontalAlignment::Stretch);for(wchar_t const* p:{L"禁用",L"Ctrl+Alt+F",L"Ctrl+Shift+F",L"Alt+Q"}){ComboBoxItem it;it.Content(box_value(i18n::Tr(p)));hotkeySearch.Items().Append(it);}
 hotkeySearch.SelectionChanged([this](auto&&,auto&&){if(applying)return;OnHotkeySearch(hotkeySearch.SelectedIndex());});panel.Children().Append(hotkeySearch);
 hotkeySearchHint=TextBlock();hotkeySearchHint.Text(i18n::Tr(L"热键已被其他程序占用，未生效。"));hotkeySearchHint.FontSize(11);hotkeySearchHint.Foreground(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Color{255,232,17,35}));hotkeySearchHint.Visibility(Visibility::Collapsed);panel.Children().Append(hotkeySearchHint);
 TextBlock searchHint;searchHint.Text(i18n::Tr(L"按热键随时唤起搜索框，快速打开分区里的内容。"));searchHint.FontSize(11);searchHint.Opacity(0.6);searchHint.TextWrapping(TextWrapping::Wrap);panel.Children().Append(searchHint);
 panel.Children().Append(Caption(i18n::Tr(L"快速捕获热键")));
 hotkeyCapture=ComboBox();hotkeyCapture.HorizontalAlignment(HorizontalAlignment::Stretch);for(wchar_t const* p:{L"禁用",L"Ctrl+Alt+V",L"Ctrl+Shift+V",L"Alt+C"}){ComboBoxItem it;it.Content(box_value(i18n::Tr(p)));hotkeyCapture.Items().Append(it);}
 hotkeyCapture.SelectionChanged([this](auto&&,auto&&){if(applying)return;OnHotkeyCapture(hotkeyCapture.SelectedIndex());});panel.Children().Append(hotkeyCapture);
 hotkeyCaptureHint=TextBlock();hotkeyCaptureHint.Text(i18n::Tr(L"热键已被其他程序占用，未生效。"));hotkeyCaptureHint.FontSize(11);hotkeyCaptureHint.Foreground(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Color{255,232,17,35}));hotkeyCaptureHint.Visibility(Visibility::Collapsed);panel.Children().Append(hotkeyCaptureHint);
 TextBlock captureHint;captureHint.Text(i18n::Tr(L"按热键随时唤起捕获框：Enter 记入便签，Ctrl+Enter 存为待办。"));captureHint.FontSize(11);captureHint.Opacity(0.6);captureHint.TextWrapping(TextWrapping::Wrap);panel.Children().Append(captureHint);
 panel.Children().Append(Caption(i18n::Tr(L"撤销热键")));
 hotkeyUndo=ComboBox();hotkeyUndo.HorizontalAlignment(HorizontalAlignment::Stretch);for(wchar_t const* p:{L"禁用",L"Ctrl+Alt+U",L"Ctrl+Alt+Z",L"Ctrl+Shift+Z"}){ComboBoxItem it;it.Content(box_value(i18n::Tr(p)));hotkeyUndo.Items().Append(it);}
 hotkeyUndo.SelectionChanged([this](auto&&,auto&&){if(applying)return;OnHotkeyUndo(hotkeyUndo.SelectedIndex());});panel.Children().Append(hotkeyUndo);
 hotkeyUndoHint=TextBlock();hotkeyUndoHint.Text(i18n::Tr(L"热键已被其他程序占用，未生效。"));hotkeyUndoHint.FontSize(11);hotkeyUndoHint.Foreground(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Color{255,232,17,35}));hotkeyUndoHint.Visibility(Visibility::Collapsed);panel.Children().Append(hotkeyUndoHint);
 TextBlock undoHint;undoHint.Text(i18n::Tr(L"删除分区、移除入口、合并标签组等改动都可按热键或托盘「撤销」回退，最多 20 步。"));undoHint.FontSize(11);undoHint.Opacity(0.6);undoHint.TextWrapping(TextWrapping::Wrap);panel.Children().Append(undoHint);
 panel.Children().Append(Caption(i18n::Tr(L"透桌面热键")));
 hotkeyReveal=ComboBox();hotkeyReveal.HorizontalAlignment(HorizontalAlignment::Stretch);for(wchar_t const* p:{L"禁用",L"Ctrl+Alt+Space",L"Ctrl+Alt+Shift+Space"}){ComboBoxItem it;it.Content(box_value(i18n::Tr(p)));hotkeyReveal.Items().Append(it);}
 hotkeyReveal.SelectionChanged([this](auto&&,auto&&){if(applying)return;OnHotkeyReveal(hotkeyReveal.SelectedIndex());});panel.Children().Append(hotkeyReveal);
 TextBlock revealHint;revealHint.Text(i18n::Tr(L"按住热键期间临时隐藏所有分区，松开即恢复，方便直接操作桌面。"));revealHint.FontSize(11);revealHint.Opacity(0.6);revealHint.TextWrapping(TextWrapping::Wrap);panel.Children().Append(revealHint);
 tabHover=ToggleSwitch();tabHover.OnContent(box_value(i18n::Tr(L"悬停切换标签")));tabHover.OffContent(box_value(i18n::Tr(L"悬停切换标签")));tabHover.Toggled([this](auto&&,auto&&){if(applying)return;OnTabHover(tabHover.IsOn());});panel.Children().Append(tabHover);
 TextBlock hoverHint;hoverHint.Text(i18n::Tr(L"鼠标停在标签组的其他标签上片刻即切换分区。"));hoverHint.FontSize(11);hoverHint.Opacity(0.6);hoverHint.TextWrapping(TextWrapping::Wrap);panel.Children().Append(hoverHint);
 memTrim=ToggleSwitch();memTrim.OnContent(box_value(i18n::Tr(L"空闲时修剪内存")));memTrim.OffContent(box_value(i18n::Tr(L"空闲时修剪内存")));memTrim.Toggled([this](auto&&,auto&&){if(applying)return;OnMemTrim(memTrim.IsOn());});panel.Children().Append(memTrim);
 TextBlock trimHint;trimHint.Text(i18n::Tr(L"长时间无操作后自动释放占用的内存，需要时再取回。"));trimHint.FontSize(11);trimHint.Opacity(0.6);trimHint.TextWrapping(TextWrapping::Wrap);panel.Children().Append(trimHint);
 panel.Children().Append(Caption(i18n::Tr(L"天气")));
 StackPanel cityBar;cityBar.Orientation(Orientation::Horizontal);cityBar.Spacing(8);
 weatherCity=TextBox();weatherCity.PlaceholderText(i18n::Tr(L"城市名，如：上海"));weatherCity.Width(150);cityBar.Children().Append(weatherCity);
 auto geoBtn=Button();geoBtn.Content(box_value(i18n::Tr(L"搜索")));geoBtn.Click([this](auto&&,auto&&){OnGeoSearch();});cityBar.Children().Append(geoBtn);
 auto autoBtn=Button();autoBtn.Content(box_value(i18n::Tr(L"自动定位")));autoBtn.Click([this](auto&&,auto&&){OnGeoAuto();});cityBar.Children().Append(autoBtn);
 panel.Children().Append(cityBar);
 weatherResults=ComboBox();weatherResults.HorizontalAlignment(HorizontalAlignment::Stretch);weatherResults.Visibility(Visibility::Collapsed);weatherResults.Margin(Thickness{0,8,0,0});weatherResults.SelectionChanged([this](auto&&,auto&&){if(weatherSave)weatherSave.Visibility(weatherResults.SelectedIndex()>=0?Visibility::Visible:Visibility::Collapsed);});panel.Children().Append(weatherResults);
 weatherSave=Button();weatherSave.Content(box_value(i18n::Tr(L"使用该城市")));weatherSave.Margin(Thickness{0,8,0,0});weatherSave.Visibility(Visibility::Collapsed);weatherSave.Click([this](auto&&,auto&&){OnGeoSave();});panel.Children().Append(weatherSave);
 weatherHint=TextBlock();weatherHint.Text(i18n::Tr(L"默认自动定位，也可以手动选择城市。"));weatherHint.FontSize(11);weatherHint.Opacity(0.6);weatherHint.TextWrapping(TextWrapping::Wrap);weatherHint.Margin(Thickness{0,4,0,0});panel.Children().Append(weatherHint);
 weatherSkin=ComboBox();weatherSkin.HorizontalAlignment(HorizontalAlignment::Stretch);weatherSkin.Margin(Thickness{0,8,0,0});for(wchar_t const* p:{L"标准皮肤",L"跟随天气状况"}){ComboBoxItem it;it.Content(box_value(i18n::Tr(p)));weatherSkin.Items().Append(it);}
 weatherSkin.SelectionChanged([this](auto&&,auto&&){if(applying)return;OnWeatherSkin(weatherSkin.SelectedIndex());});panel.Children().Append(weatherSkin);
 TextBlock skinHint;skinHint.Text(i18n::Tr(L"状况皮肤会按晴、雨、雪、雷暴等切换背景渐变色。"));skinHint.FontSize(11);skinHint.Opacity(0.6);skinHint.TextWrapping(TextWrapping::Wrap);panel.Children().Append(skinHint);
 panel.Children().Append(Caption(i18n::Tr(L"语言")));
 lang=ComboBox();lang.HorizontalAlignment(HorizontalAlignment::Stretch);ComboBoxItem langSys;langSys.Content(box_value(i18n::Tr(L"跟随系统")));lang.Items().Append(langSys);ComboBoxItem langZh;langZh.Content(box_value(L"简体中文"));lang.Items().Append(langZh);ComboBoxItem langEn;langEn.Content(box_value(L"English"));lang.Items().Append(langEn);
 lang.SelectionChanged([this](auto&&,auto&&){if(applying)return;OnLanguage(lang.SelectedIndex());});panel.Children().Append(lang);
 TextBlock langHint;langHint.Text(i18n::Tr(L"语言将在重启 GuoDesk 后生效。"));langHint.FontSize(11);langHint.Opacity(0.6);panel.Children().Append(langHint);
 panel.Children().Append(Caption(i18n::Tr(L"备份")));
 StackPanel backupBar;backupBar.Orientation(Orientation::Horizontal);backupBar.Spacing(8);
 auto exportBtn=Button();exportBtn.Content(box_value(i18n::Tr(L"导出配置")));exportBtn.Click([this](auto&&,auto&&){OnExport();});backupBar.Children().Append(exportBtn);
 auto importBtn=Button();importBtn.Content(box_value(i18n::Tr(L"导入配置")));importBtn.Click([this](auto&&,auto&&){OnImport();});backupBar.Children().Append(importBtn);
 auto diagBtn=Button();diagBtn.Content(box_value(i18n::Tr(L"导出诊断…")));diagBtn.Click([this](auto&&,auto&&){OnDiagnostics();});backupBar.Children().Append(diagBtn);
 panel.Children().Append(backupBar);
 snapshots=ToggleSwitch();snapshots.OnContent(box_value(i18n::Tr(L"配置自动快照")));snapshots.OffContent(box_value(i18n::Tr(L"配置自动快照")));snapshots.Toggled([this](auto&&,auto&&){if(applying)return;OnSnapshots(snapshots.IsOn());});panel.Children().Append(snapshots);
 TextBlock snapHint;snapHint.Text(i18n::Tr(L"每天首次及每 20 次保存各留一份，保留最近 5 份，配置损坏可自动恢复。"));snapHint.FontSize(11);snapHint.Opacity(0.6);snapHint.TextWrapping(TextWrapping::Wrap);panel.Children().Append(snapHint);
 auto guideLink=HyperlinkButton();guideLink.Content(box_value(i18n::Tr(L"查看新手引导")));guideLink.Margin(Thickness{0,2,0,0});guideLink.Padding(Thickness{0});guideLink.Click([this](auto&&,auto&&){owner.ShowGuide();});panel.Children().Append(guideLink);
 panel.Children().Append(Caption(i18n::Tr(L"分区模板")));
 TextBlock tplIntro;tplIntro.Text(i18n::Tr(L"一键在屏幕工作区铺好常用文件夹分区；已有对应文件夹的分区会自动跳过，不会重复添加。"));tplIntro.FontSize(12);tplIntro.TextWrapping(TextWrapping::Wrap);tplIntro.Opacity(0.8);panel.Children().Append(tplIntro);
 StackPanel tplBar;tplBar.Orientation(Orientation::Horizontal);tplBar.Spacing(8);tplBar.Margin(Thickness{0,8,0,0});
 {auto tpls=BuiltInTemplates();for(size_t i=0;i<tpls.size();++i){auto b=Button();b.Content(box_value(i18n::Tr(tpls[i].name)));auto const tpl=tpls[i];b.Click([this,tpl](auto&&,auto&&){owner.UseTemplate(tpl);});tplBar.Children().Append(b);}}
 panel.Children().Append(tplBar);
 panel.Children().Append(Caption(i18n::Tr(L"WebDAV 同步")));
 TextBlock syncIntro;syncIntro.Text(i18n::Tr(L"通过任意支持 WebDAV 的网盘在多台电脑间同步分区配置，上传后以 guodesk-layout.json 存到该目录。"));syncIntro.FontSize(12);syncIntro.TextWrapping(TextWrapping::Wrap);syncIntro.Opacity(0.8);panel.Children().Append(syncIntro);
 syncUrl=TextBox();syncUrl.PlaceholderText(i18n::Tr(L"WebDAV 地址，如 https://dav.jianguoyun.com/dav/GuoDesk"));syncUrl.HorizontalAlignment(HorizontalAlignment::Stretch);syncUrl.Margin(Thickness{0,6,0,0});panel.Children().Append(syncUrl);
 syncUser=TextBox();syncUser.PlaceholderText(i18n::Tr(L"账号（可选）"));syncUser.HorizontalAlignment(HorizontalAlignment::Stretch);syncUser.Margin(Thickness{0,6,0,0});panel.Children().Append(syncUser);
 syncPass=PasswordBox();syncPass.PlaceholderText(i18n::Tr(L"密码（本机加密保存）"));syncPass.HorizontalAlignment(HorizontalAlignment::Stretch);syncPass.Margin(Thickness{0,6,0,0});panel.Children().Append(syncPass);
 syncAuto=ToggleSwitch();syncAuto.OnContent(box_value(i18n::Tr(L"自动同步")));syncAuto.OffContent(box_value(i18n::Tr(L"自动同步")));syncAuto.Margin(Thickness{0,6,0,0});syncAuto.Toggled([this](auto&&,auto&&){if(applying)return;OnSyncAuto(syncAuto.IsOn());});panel.Children().Append(syncAuto);
 syncInsecure=ToggleSwitch();syncInsecure.OnContent(box_value(i18n::Tr(L"忽略证书错误（仅受信网络）")));syncInsecure.OffContent(box_value(i18n::Tr(L"忽略证书错误（仅受信网络）")));syncInsecure.Margin(Thickness{0,6,0,0});syncInsecure.Toggled([this](auto&&,auto&&){if(applying)return;owner.layout.settings.syncInsecure=syncInsecure.IsOn();owner.Save();});panel.Children().Append(syncInsecure);
 StackPanel syncBar;syncBar.Orientation(Orientation::Horizontal);syncBar.Spacing(8);syncBar.Margin(Thickness{0,8,0,0});
 auto syncUpBtn=Button();syncUpBtn.Content(box_value(i18n::Tr(L"上传到云端")));syncUpBtn.Click([this](auto&&,auto&&){OnSyncUpload();});syncBar.Children().Append(syncUpBtn);
 auto syncDownBtn=Button();syncDownBtn.Content(box_value(i18n::Tr(L"从云端恢复")));syncDownBtn.Click([this](auto&&,auto&&){OnSyncDownload();});syncBar.Children().Append(syncDownBtn);
 panel.Children().Append(syncBar);
 syncHint=TextBlock();syncHint.FontSize(11);syncHint.Opacity(0.85);syncHint.TextWrapping(TextWrapping::Wrap);syncHint.Margin(Thickness{0,4,0,0});panel.Children().Append(syncHint);
 panel.Children().Append(Caption(i18n::Tr(L"整理规则")));
 TextBlock ruleIntro;ruleIntro.Text(i18n::Tr(L"按扩展名或文件名关键词，把桌面文件以引用方式归入分区——原文件始终保持在桌面。"));ruleIntro.FontSize(12);ruleIntro.TextWrapping(TextWrapping::Wrap);ruleIntro.Opacity(0.8);panel.Children().Append(ruleIntro);
 rulesPanel=StackPanel();rulesPanel.Spacing(4);rulesPanel.Padding(Thickness{0,6,0,0});panel.Children().Append(rulesPanel);
 StackPanel ruleBar;ruleBar.Orientation(Orientation::Horizontal);ruleBar.Spacing(8);ruleBar.Margin(Thickness{0,8,0,0});
 auto addRule=Button();addRule.Content(box_value(i18n::Tr(L"添加规则")));addRule.Click([this](auto&&,auto&&){EditRule(L"");});ruleBar.Children().Append(addRule);
 auto tidyBtn=Button();tidyBtn.Content(box_value(i18n::Tr(L"预览整理…")));tidyBtn.Click([this](auto&&,auto&&){owner.ShowTidy();});ruleBar.Children().Append(tidyBtn);
 panel.Children().Append(ruleBar);
 RebuildRules();
 panel.Children().Append(Caption(i18n::Tr(L"关于")));
 TextBlock about;auto aboutText=i18n::Tr(L"GuoDesk · 桌面分区整理\n引用式入口：只存引用，不动原文件\n缺失入口可右键重新定位\n便签与待办：托盘右键开启，待办可设截止日期提醒\n时钟：托盘右键开启，右键时钟查看日历\n音乐·搜索·天气：托盘右键开启\n双击 Ctrl 或自定义热键随时唤起\n分区模板：托盘或设置一键铺好常用文件夹分区\n映射分区可就地浏览，面包屑返回\n快速捕获：Enter 记便签，Ctrl+Enter 存待办\n组件字号三档可调，时钟支持数字与模拟表盘\n搜索窗可直连 Everything，秒级检索本地文件\n分区主题色与背景图：右键菜单随时换装\n误操作可撤销：Ctrl+Alt+U 或托盘，最多 20 步\n标签组支持悬停秒切，跨分区拖动也能回退\nWebDAV 同步：设置中配置网盘，多机同步布局\n\nMIT License · cloudlight369");aboutText.replace(0,8,L"GuoDesk v"+DiagVersion()+L" ");about.Text(aboutText);about.FontSize(12);about.TextWrapping(TextWrapping::Wrap);about.Opacity(0.8);panel.Children().Append(about);
 scroll.Content(panel);window.Content(scroll);
 window.Closed([this](auto&&,auto&&){if(closing)return;closing=true;window.DispatcherQueue().TryEnqueue([this]{owner.CloseSettings();});});
 window.Activate();
}
void SettingsWindow::OnTheme(int index){auto& s=owner.layout.settings;s.theme=index==1?L"Light":index==2?L"Dark":L"";owner.ApplySettings();owner.Save();}
void SettingsWindow::OnCompact(bool on){owner.layout.settings.compact=on;owner.ApplySettings();owner.Save();}
void SettingsWindow::OnPerformance(bool on){owner.layout.settings.performance=on;for(auto& w:owner.windows)w->ApplySettings();owner.Save();}
void SettingsWindow::OnEverything(bool on){owner.layout.settings.everything=on;owner.Save();}
void SettingsWindow::OnAutostart(bool on){SetAutostart(on);bool actual=AutostartEnabled();if(actual!=on){applying=true;autostart.IsOn(actual);applying=false;}}
void SettingsWindow::OnLanguage(int index){owner.layout.settings.language=index==1?L"zh-CN":index==2?L"en-US":L"";owner.Save();}
void SettingsWindow::OnTextSize(int index){index=std::clamp(index,0,2);if(index==owner.layout.settings.textSize)return;owner.layout.settings.textSize=index;owner.Save();owner.RebuildWidgets();}
void SettingsWindow::OnClockStyle(int index){std::wstring v=index==1?L"analog":L"";if(v==owner.layout.settings.clockStyle)return;owner.layout.settings.clockStyle=v;owner.Save();owner.RebuildWidgets();}
void SettingsWindow::OnBackdrop(int index){index=ClampBackdropKind(index);if(index==owner.layout.settings.backdrop)return;owner.layout.settings.backdrop=index;owner.Save();owner.RebuildWidgets();}
void SettingsWindow::OnWeatherSkin(int index){int v=index==1?1:0;if(v==owner.layout.widgets.weatherSkin)return;owner.layout.widgets.weatherSkin=v;owner.Save();if(owner.weather)owner.weather->Reload();}
void SettingsWindow::OnHotkey(int index){auto& s=owner.layout.settings;if(index==5){hotkeyHint.Visibility(Visibility::Collapsed);window.DispatcherQueue().TryEnqueue([hc=hotkeyCustom]{hc.Focus(FocusState::Programmatic);});return;}s.hotkey=index==0?L"DoubleCtrl":index==1?L"Ctrl+Alt+G":index==2?L"Ctrl+Alt+Z":index==3?L"Ctrl+Shift+Space":index==4?L"Win+Z":L"";hotkeyCustom.Text(s.hotkey==L"DoubleCtrl"?i18n::Tr(L"双击 Ctrl"):s.hotkey);bool ok=owner.ApplyHotkey();hotkeyHint.Text(i18n::Tr(L"热键已被其他程序占用，未生效。"));hotkeyHint.Visibility(ok?Visibility::Collapsed:Visibility::Visible);owner.Save();}
void SettingsWindow::OnHotkeySearch(int index){auto& s=owner.layout.settings;s.hotkeySearch=index==1?L"Ctrl+Alt+F":index==2?L"Ctrl+Shift+F":index==3?L"Alt+Q":L"";bool ok=owner.ApplyHotkey();hotkeySearchHint.Visibility(ok?Visibility::Collapsed:Visibility::Visible);owner.Save();}
void SettingsWindow::OnHotkeyCapture(int index){auto& s=owner.layout.settings;s.hotkeyCapture=index==1?L"Ctrl+Alt+V":index==2?L"Ctrl+Shift+V":index==3?L"Alt+C":L"";bool ok=owner.ApplyHotkey();hotkeyCaptureHint.Visibility(ok?Visibility::Collapsed:Visibility::Visible);owner.Save();}
void SettingsWindow::OnHotkeyUndo(int index){auto& s=owner.layout.settings;s.hotkeyUndo=index==1?L"Ctrl+Alt+U":index==2?L"Ctrl+Alt+Z":index==3?L"Ctrl+Shift+Z":L"";bool ok=owner.ApplyHotkey();hotkeyUndoHint.Visibility(ok?Visibility::Collapsed:Visibility::Visible);owner.Save();}
void SettingsWindow::OnHotkeyReveal(int index){auto& s=owner.layout.settings;s.revealHotkey=index==1?L"Ctrl+Alt+Space":index==2?L"Ctrl+Alt+Shift+Space":L"";owner.ApplyHotkey();owner.Save();}
void SettingsWindow::OnTabHover(bool on){owner.layout.settings.tabHover=on;owner.ApplySettings();owner.Save();}
void SettingsWindow::OnMemTrim(bool on){owner.layout.settings.memTrim=on;owner.Save();}
void SettingsWindow::OnGeoSearch(){auto name=std::wstring(weatherCity.Text());size_t a=name.find_first_not_of(L" \t");size_t b=name.find_last_not_of(L" \t");name=a==std::wstring::npos?L"":name.substr(a,b-a+1);if(name.empty())return;weatherHint.Text(i18n::Tr(L"正在搜索…"));weatherResults.Visibility(Visibility::Collapsed);weatherSave.Visibility(Visibility::Collapsed);auto weak=std::weak_ptr<bool>(alive);std::thread([this,weak,name]{auto json=HttpGetJson(L"geocoding-api.open-meteo.com",L"/v1/search?count=5&language=zh-CN&format=json&name="+UrlParam(name),false);auto list=ParseGeoJson(std::wstring(winrt::to_hstring(json)));window.DispatcherQueue().TryEnqueue([this,weak,list]{if(weak.lock()==nullptr||closing||!weatherResults)return;geo=list;weatherResults.Items().Clear();for(auto const& g:list)weatherResults.Items().Append(box_value(g.country.empty()?g.name:g.name+L" · "+g.country));weatherResults.Visibility(Visibility::Visible);weatherHint.Text(list.empty()?i18n::Tr(L"没有找到这个城市，换个名字试试。"):i18n::Tr(L"选择一个城市后点击“使用该城市”。"));weatherSave.Visibility(Visibility::Collapsed);});}).detach();}
void SettingsWindow::OnGeoSave(){int sel=weatherResults.SelectedIndex();if(sel<0||sel>=static_cast<int>(geo.size()))return;auto& s=owner.layout.widgets;s.weatherCity=geo[static_cast<size_t>(sel)].name;s.weatherLat=geo[static_cast<size_t>(sel)].lat;s.weatherLon=geo[static_cast<size_t>(sel)].lon;owner.Save();weatherHint.Text(i18n::Tr(L"已保存，天气组件将使用所选城市。"));weatherResults.Visibility(Visibility::Collapsed);weatherSave.Visibility(Visibility::Collapsed);if(owner.weather)owner.weather->Reload();}
void SettingsWindow::OnGeoAuto(){auto& s=owner.layout.widgets;s.weatherCity.clear();s.weatherLat=999;s.weatherLon=999;owner.Save();weatherCity.Text(L"");weatherHint.Text(i18n::Tr(L"已恢复自动定位。"));weatherResults.Visibility(Visibility::Collapsed);weatherSave.Visibility(Visibility::Collapsed);if(owner.weather)owner.weather->Reload();}
void SettingsWindow::OnSnapshots(bool on){owner.layout.settings.snapshots=on;owner.store.SetSnapshots(on);owner.Save();}
static std::wstring TrimSpace(std::wstring v){size_t a=v.find_first_not_of(L" \t\r\n");if(a==std::wstring::npos)return{};size_t b=v.find_last_not_of(L" \t\r\n");return v.substr(a,b-a+1);}
static void SyncHintWarn(winrt::Microsoft::UI::Xaml::Controls::TextBlock const& hint,std::wstring const& text){hint.Foreground(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Color{255,232,17,35}));hint.Text(text);}
static void SyncHintInfo(winrt::Microsoft::UI::Xaml::Controls::TextBlock const& hint,std::wstring const& text){hint.Foreground(nullptr);hint.Text(text);}
void SettingsWindow::OnSyncSave(){
 auto& s=owner.layout.settings;
 s.syncUrl=TrimSpace(std::wstring(syncUrl.Text()));
 if(!s.syncUrl.empty()){webdav::UrlParts up;if(!webdav::ParseUrl(s.syncUrl,up)){s.syncUrl.clear();SyncHintWarn(syncHint,i18n::Tr(L"地址无效：需以 http:// 或 https:// 开头。"));return;}}
 s.syncUser=TrimSpace(std::wstring(syncUser.Text()));
 auto pw=TrimSpace(std::wstring(syncPass.Password()));
 if(!pw.empty()){auto prot=webdav::ProtectSecret(pw);if(prot.empty()){SyncHintWarn(syncHint,i18n::Tr(L"密码加密失败（系统凭据保护不可用），已保留原密码。"));owner.Save();return;}s.syncPass=prot;}
 else s.syncPass.clear();
 owner.Save();
}
void SettingsWindow::OnSyncAuto(bool on){
 OnSyncSave();
 auto& s=owner.layout.settings;
 if(on&&(s.syncUrl.empty()||s.syncPass.empty())){SyncHintWarn(syncHint,i18n::Tr(L"请先填写 WebDAV 地址和密码，再开启自动同步。"));applying=true;syncAuto.IsOn(false);applying=false;return;}
 s.syncAuto=on;owner.Save();
 if(on)SyncHintInfo(syncHint,i18n::Tr(L"已开启自动同步：配置变化后约 20 秒内自动上传。"));
}
void SettingsWindow::OnSyncUpload(){
 OnSyncSave();
 if(owner.layout.settings.syncUrl.empty()||owner.layout.settings.syncPass.empty()){SyncHintWarn(syncHint,i18n::Tr(L"请先填写 WebDAV 地址和密码。"));return;}
 SyncHintInfo(syncHint,i18n::Tr(L"正在上传…"));
 auto data=Serialize(owner.layout);
 auto url=webdav::JoinUrl(owner.layout.settings.syncUrl,L"guodesk-layout.json");
 auto user=owner.layout.settings.syncUser;auto pass=webdav::UnprotectSecret(owner.layout.settings.syncPass);
 auto insecure=owner.layout.settings.syncInsecure;
 auto weak=std::weak_ptr<bool>(alive);
 std::thread([this,weak,data=std::move(data),url,user,pass,insecure](){
  bool ok=webdav::UploadText(url,user,pass,data,insecure);
  window.DispatcherQueue().TryEnqueue([this,weak,ok]{if(weak.lock()==nullptr||closing||!syncHint)return;SyncHintInfo(syncHint,ok?i18n::Tr(L"上传完成。"):i18n::Tr(L"上传失败：请检查地址、账号密码或网络。"));});
 }).detach();
}
void SettingsWindow::OnSyncDownload(){
 OnSyncSave();
 if(owner.layout.settings.syncUrl.empty()||owner.layout.settings.syncPass.empty()){SyncHintWarn(syncHint,i18n::Tr(L"请先填写 WebDAV 地址和密码。"));return;}
 SyncHintInfo(syncHint,i18n::Tr(L"正在下载…"));
 auto url=webdav::JoinUrl(owner.layout.settings.syncUrl,L"guodesk-layout.json");
 auto user=owner.layout.settings.syncUser;auto pass=webdav::UnprotectSecret(owner.layout.settings.syncPass);
 auto insecure=owner.layout.settings.syncInsecure;
 auto weak=std::weak_ptr<bool>(alive);
 std::thread([this,weak,url,user,pass,insecure](){
  std::string data;bool ok=webdav::DownloadText(url,user,pass,data,insecure);
  Layout next;bool valid=false;
  if(ok){try{next=Deserialize(data);valid=true;}catch(...){}}
  window.DispatcherQueue().TryEnqueue([this,weak,ok,valid,next=std::move(next)]()mutable{
   if(weak.lock()==nullptr||closing||!syncHint)return;
   if(!ok){SyncHintWarn(syncHint,i18n::Tr(L"下载失败：请检查地址、账号密码或网络。"));return;}
   if(!valid){SyncHintWarn(syncHint,i18n::Tr(L"云端文件不是有效的 GuoDesk 配置。"));return;}
   owner.ImportLayout(std::move(next));
  });
 }).detach();
}
void SettingsWindow::Apply(){applying=true;auto const& s=owner.layout.settings;theme.SelectedIndex(s.theme==L"Light"?1:s.theme==L"Dark"?2:0);compact.IsOn(s.compact);performance.IsOn(s.performance);autostart.IsOn(AutostartEnabled());everythingToggle.IsOn(s.everything);evHint.Text(ev::Available()?i18n::Tr(L"已检测到 Everything，搜索窗会附带本地文件结果。"):i18n::Tr(L"未检测到正在运行的 Everything，安装并启动后搜索窗可附带本地文件结果。"));lang.SelectedIndex(s.language==L"en-US"?2:s.language==L"zh-CN"?1:0);hotkey.SelectedIndex(s.hotkey==L"DoubleCtrl"?0:s.hotkey==L"Ctrl+Alt+G"?1:s.hotkey==L"Ctrl+Alt+Z"?2:s.hotkey==L"Ctrl+Shift+Space"?3:s.hotkey==L"Win+Z"?4:s.hotkey.empty()?6:5);hotkeyCustom.Text(s.hotkey==L"DoubleCtrl"?i18n::Tr(L"双击 Ctrl"):s.hotkey);hotkeySearch.SelectedIndex(s.hotkeySearch==L"Ctrl+Alt+F"?1:s.hotkeySearch==L"Ctrl+Shift+F"?2:s.hotkeySearch==L"Alt+Q"?3:0);hotkeyCapture.SelectedIndex(s.hotkeyCapture==L"Ctrl+Alt+V"?1:s.hotkeyCapture==L"Ctrl+Shift+V"?2:s.hotkeyCapture==L"Alt+C"?3:0);hotkeyUndo.SelectedIndex(s.hotkeyUndo==L"Ctrl+Alt+U"?1:s.hotkeyUndo==L"Ctrl+Alt+Z"?2:s.hotkeyUndo==L"Ctrl+Shift+Z"?3:0);hotkeyReveal.SelectedIndex(s.revealHotkey==L"Ctrl+Alt+Space"?1:s.revealHotkey==L"Ctrl+Alt+Shift+Space"?2:0);tabHover.IsOn(s.tabHover);memTrim.IsOn(s.memTrim);textSize.SelectedIndex(s.textSize<0?0:s.textSize>2?2:s.textSize);clockStyle.SelectedIndex(s.clockStyle==L"analog"?1:0);backdrop.SelectedIndex(ClampBackdropKind(s.backdrop));weatherSkin.SelectedIndex(owner.layout.widgets.weatherSkin==1?1:0);weatherCity.Text(owner.layout.widgets.weatherCity);snapshots.IsOn(s.snapshots);syncUrl.Text(s.syncUrl);syncUser.Text(s.syncUser);try{syncPass.Password(webdav::UnprotectSecret(s.syncPass));}catch(...){}syncAuto.IsOn(s.syncAuto);syncInsecure.IsOn(s.syncInsecure);if(syncHint){syncHint.Foreground(nullptr);syncHint.Text(L"");}hotkeyHint.Visibility(Visibility::Collapsed);hotkeySearchHint.Visibility(Visibility::Collapsed);hotkeyCaptureHint.Visibility(Visibility::Collapsed);hotkeyUndoHint.Visibility(Visibility::Collapsed);applying=false;}
static std::string ReadTextFile(std::filesystem::path const& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Cannot read file");return {std::istreambuf_iterator<char>(f),{}};}
void SettingsWindow::OnExport(){owner.Save();auto target=shell::SaveFile(hwnd,L"guodesk-layout.json");if(target.empty())return;try{std::filesystem::copy_file(owner.store.Directory()/L"layout.json",target,std::filesystem::copy_options::overwrite_existing);}catch(...){MessageBoxW(hwnd,i18n::Tr(L"导出失败：请检查目标位置是否可写。").c_str(),L"GuoDesk",MB_OK|MB_ICONERROR);return;}if(!owner.windows.empty())owner.windows.front()->Notify(i18n::TrF(L"已导出到 {0}",{target}));}
void SettingsWindow::OnImport(){auto picked=shell::Pick(hwnd,false,i18n::Tr(L"选择要导入的 GuoDesk 配置"));if(picked.size()!=1)return;std::string text;try{text=ReadTextFile(std::filesystem::path(picked[0]));}catch(...){MessageBoxW(hwnd,i18n::Tr(L"导入失败：文件不是有效的 GuoDesk 配置。").c_str(),L"GuoDesk",MB_OK|MB_ICONERROR);return;}Layout next;try{next=Deserialize(text);}catch(...){MessageBoxW(hwnd,i18n::Tr(L"导入失败：文件不是有效的 GuoDesk 配置。").c_str(),L"GuoDesk",MB_OK|MB_ICONERROR);return;}owner.ImportLayout(std::move(next));Controller* c=&owner;winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread().TryEnqueue([c](){c->CloseSettings();});}
static std::wstring DiagVersion(){
 wchar_t path[MAX_PATH]{};GetModuleFileNameW(nullptr,path,MAX_PATH);std::wstring out=L"unknown";
 if(HMODULE v=LoadLibraryW(L"version.dll")){
  using GSZ=DWORD(WINAPI*)(LPCWSTR,DWORD*);using GFI=BOOL(WINAPI*)(LPCWSTR,DWORD,DWORD,LPVOID);using QV=BOOL(WINAPI*)(LPVOID,LPCWSTR,LPVOID*,UINT*);
  auto gsz=(GSZ)(void*)GetProcAddress(v,"GetFileVersionInfoSizeW");auto gfi=(GFI)(void*)GetProcAddress(v,"GetFileVersionInfoW");auto qv=(QV)(void*)GetProcAddress(v,"VerQueryValueW");
  if(gsz&&gfi&&qv)if(DWORD sz=gsz(path,nullptr);sz>0){std::vector<char> buf(sz);if(gfi(path,0,sz,buf.data())){VS_FIXEDFILEINFO* ffi{};UINT len=0;if(qv(buf.data(),L"\\",(void**)&ffi,&len)&&len){wchar_t b[32];swprintf_s(b,32,L"%u.%u.%u.%u",HIWORD(ffi->dwFileVersionMS),LOWORD(ffi->dwFileVersionMS),HIWORD(ffi->dwFileVersionLS),LOWORD(ffi->dwFileVersionLS));out=b;}}}
  FreeLibrary(v);
 }
 return out;
}
static std::wstring DiagOs(){
 std::wstring build;wchar_t buf[64]{};DWORD c=sizeof(buf);
 if(ERROR_SUCCESS==RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",L"CurrentBuildNumber",RRF_RT_REG_SZ,nullptr,buf,&c)){build=buf;wchar_t dv[64]{};c=sizeof(dv);if(ERROR_SUCCESS==RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",L"DisplayVersion",RRF_RT_REG_SZ,nullptr,dv,&c)&&*dv)build+=L"."+std::wstring(dv);}
 return build.empty()?std::wstring():L"Windows 10/11 "+build;
}
void SettingsWindow::OnDiagnostics(){
 owner.Save();
 SYSTEMTIME st{};GetLocalTime(&st);wchar_t nm[48]{};swprintf_s(nm,48,L"guodesk-diag-%04d%02d%02d-%02d%02d%02d.txt",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
 auto target=shell::SaveFile(hwnd,nm);if(target.empty())return;
#if defined(_M_ARM64)
 std::wstring machine=L"ARM64";
#elif defined(_M_X64)
 std::wstring machine=L"x64";
#else
 std::wstring machine=L"x86";
#endif
 auto text=BuildDiagnostics(owner.layout,DiagVersion(),machine,DiagOs(),static_cast<long long>(st.wYear)*10000+st.wMonth*100+st.wDay);
 try{std::ofstream f(target,std::ios::binary|std::ios::trunc);if(!f)throw std::runtime_error("open");f.write("\xef\xbb\xbf",3);f<<winrt::to_string(text);if(!f)throw std::runtime_error("write");}catch(...){MessageBoxW(hwnd,i18n::Tr(L"导出失败：请检查目标位置是否可写。").c_str(),L"GuoDesk",MB_OK|MB_ICONERROR);return;}
 if(!owner.windows.empty())owner.windows.front()->Notify(i18n::TrF(L"已导出到 {0}",{target}));
}
void SettingsWindow::Show(){window.Activate();}
SettingsWindow::~SettingsWindow(){closing=true;try{window.Closed(nullptr);}catch(...){}if(IsWindow(hwnd))window.Close();}
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
