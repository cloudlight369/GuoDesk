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
 winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch everythingToggle{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock evHint{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox lang{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox textSize{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox clockStyle{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox backdrop{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox hotkey{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock hotkeyHint{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBox hotkeyCustom{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox hotkeySearch{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock hotkeySearchHint{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox hotkeyCapture{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock hotkeyCaptureHint{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox hotkeyUndo{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox hotkeyReveal{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock hotkeyUndoHint{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch tabHover{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch memTrim{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBox weatherCity{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox weatherResults{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Button weatherSave{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock weatherHint{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox weatherSkin{nullptr};
 std::vector<GeoPlace> geo;
 winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch snapshots{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBox syncUrl{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBox syncUser{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::PasswordBox syncPass{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch syncAuto{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock syncHint{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ScrollViewer scroll{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::StackPanel rulesPanel{nullptr};
 void OnTheme(int index); void OnCompact(bool on); void OnPerformance(bool on); void OnAutostart(bool on); void OnLanguage(int index); void OnEverything(bool on);
 void OnTextSize(int index); void OnClockStyle(int index); void OnWeatherSkin(int index); void OnBackdrop(int index);
 void OnHotkey(int index); void OnSnapshots(bool on);
 void OnHotkeySearch(int index); void OnHotkeyCapture(int index); void OnHotkeyUndo(int index); void OnHotkeyReveal(int index); void OnTabHover(bool on); void OnMemTrim(bool on); void OnGeoSearch(); void OnGeoSave(); void OnGeoAuto();
 void OnSyncSave(); void OnSyncUpload(); void OnSyncDownload(); void OnSyncAuto(bool on);
 void RebuildRules(); void EditRule(std::wstring ruleId);
 void OnExport(); void OnImport(); void OnDiagnostics();
public:
 SettingsWindow(Controller&);
 ~SettingsWindow();
 void Show(); void Apply();
};
}
