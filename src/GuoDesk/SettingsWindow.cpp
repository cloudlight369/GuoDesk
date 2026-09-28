#include "pch.h"
#include "SettingsWindow.h"
#include "DeskWindow.h"
#include "Shell.h"
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
namespace guodesk {
static std::wstring ExePath(){wchar_t buf[MAX_PATH]{};GetModuleFileNameW(nullptr,buf,MAX_PATH);return buf;}
static bool AutostartEnabled(){HKEY k{};if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_READ,&k)!=ERROR_SUCCESS)return false;DWORD type{},size{};LONG rc=RegQueryValueExW(k,L"GuoDesk",nullptr,&type,nullptr,&size);RegCloseKey(k);return rc==ERROR_SUCCESS&&(type==REG_SZ||type==REG_EXPAND_SZ)&&size>2;}
static void SetAutostart(bool on){HKEY k{};if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_SET_VALUE,&k)!=ERROR_SUCCESS){MessageBoxW(nullptr,L"无法写入注册表，请检查权限。",L"GuoDesk",MB_OK|MB_ICONERROR);return;}if(on){auto quoted=L"\""+ExePath()+L"\"";RegSetValueExW(k,L"GuoDesk",0,REG_SZ,reinterpret_cast<BYTE const*>(quoted.c_str()),static_cast<DWORD>((quoted.size()+1)*sizeof(wchar_t)));}else RegDeleteValueW(k,L"GuoDesk");RegCloseKey(k);}
static TextBlock Caption(wchar_t const* text){TextBlock t;t.Text(text);t.FontSize(13);t.Margin(Thickness{0,14,0,6});t.Opacity(0.7);return t;}
SettingsWindow::SettingsWindow(Controller& c):owner(c){
 window=Window();window.Title(L"GuoDesk 设置");hwnd=shell::Handle(window);
 try{auto dir=std::filesystem::path(ExePath()).parent_path();window.AppWindow().SetIcon((dir/L"guodesk.ico").wstring());}catch(...){}
 ScrollViewer scroll;StackPanel panel;panel.Padding(Thickness{24,16,24,24});panel.MaxWidth(420);panel.HorizontalAlignment(HorizontalAlignment::Left);
 panel.Children().Append(Caption(L"外观"));
 theme=ComboBox();theme.HorizontalAlignment(HorizontalAlignment::Stretch);ComboBoxItem def;def.Content(box_value(L"跟随系统"));theme.Items().Append(def);ComboBoxItem light;light.Content(box_value(L"浅色"));theme.Items().Append(light);ComboBoxItem dark;dark.Content(box_value(L"深色"));theme.Items().Append(dark);
 theme.SelectionChanged([this](auto&&,auto&&){if(applying)return;OnTheme(theme.SelectedIndex());});panel.Children().Append(theme);
 compact=ToggleSwitch();compact.OnContent(box_value(L"紧凑磁贴"));compact.OffContent(box_value(L"紧凑磁贴"));compact.Toggled([this](auto&&,auto&&){if(applying)return;OnCompact(compact.IsOn());});panel.Children().Append(compact);
 panel.Children().Append(Caption(L"常规"));
 autostart=ToggleSwitch();autostart.OnContent(box_value(L"开机自动启动"));autostart.OffContent(box_value(L"开机自动启动"));autostart.Toggled([this](auto&&,auto&&){if(applying)return;OnAutostart(autostart.IsOn());});panel.Children().Append(autostart);
 panel.Children().Append(Caption(L"关于"));
 TextBlock about;about.Text(L"GuoDesk v0.2.0 · 桌面分区整理\n引用式入口：只存引用，不动原文件\n缺失入口可右键重新定位\n\nMIT License · cloudlight369");about.FontSize(12);about.TextWrapping(TextWrapping::Wrap);about.Opacity(0.8);panel.Children().Append(about);
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
}
