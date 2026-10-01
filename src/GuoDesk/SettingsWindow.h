#pragma once
#include "Core.h"
namespace guodesk {
class Controller;
class SettingsWindow {
 Controller& owner; bool closing=false,applying=false;
 std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox theme{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch compact{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch performance{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch autostart{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox lang{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox hotkey{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock hotkeyHint{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox hotkeySearch{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock hotkeySearchHint{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBox weatherCity{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox weatherResults{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Button weatherSave{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock weatherHint{nullptr};
 std::vector<GeoPlace> geo;
 winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch snapshots{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBox syncUrl{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBox syncUser{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::PasswordBox syncPass{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch syncAuto{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock syncHint{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ScrollViewer scroll{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::StackPanel rulesPanel{nullptr};
 void OnTheme(int index); void OnCompact(bool on); void OnPerformance(bool on); void OnAutostart(bool on); void OnLanguage(int index);
 void OnHotkey(int index); void OnSnapshots(bool on);
 void OnHotkeySearch(int index); void OnGeoSearch(); void OnGeoSave(); void OnGeoAuto();
 void OnSyncSave(); void OnSyncUpload(); void OnSyncDownload(); void OnSyncAuto(bool on);
 void RebuildRules(); void EditRule(std::wstring ruleId);
 void OnExport(); void OnImport();
public:
 SettingsWindow(Controller&);
 ~SettingsWindow();
 void Show(); void Apply();
};
}
