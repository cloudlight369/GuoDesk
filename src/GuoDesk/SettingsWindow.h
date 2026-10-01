#pragma once
#include "Core.h"
namespace guodesk {
class Controller;
class SettingsWindow {
 Controller& owner; bool closing=false,applying=false;
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox theme{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch compact{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch autostart{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox lang{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox hotkey{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock hotkeyHint{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch snapshots{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ScrollViewer scroll{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::StackPanel rulesPanel{nullptr};
 void OnTheme(int index); void OnCompact(bool on); void OnAutostart(bool on); void OnLanguage(int index);
 void OnHotkey(int index); void OnSnapshots(bool on);
 void RebuildRules(); void EditRule(std::wstring ruleId);
 void OnExport(); void OnImport();
public:
 SettingsWindow(Controller&);
 ~SettingsWindow();
 void Show(); void Apply();
};
}
