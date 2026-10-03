#pragma once
#include "Core.h"
namespace guodesk {
class Controller;
class SearchWindow {
 Controller& owner; bool closing=false;
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
 winrt::Microsoft::UI::Xaml::Controls::Grid root{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBox query{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::StackPanel results{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ScrollViewer resultsHost{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock hint{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Button favToggle{nullptr};
 std::vector<SearchHit> hits;
 std::shared_ptr<bool> alive=std::make_shared<bool>(true); unsigned evGen=0;
 winrt::Microsoft::UI::Xaml::Media::Brush ThemeBrush(wchar_t const* key,winrt::Windows::UI::Color fallback);
 void Rebuild(std::wstring const& q);
 void AppendEverything(std::wstring const& q);
 void OpenPath(std::wstring const& path);
 void OpenHit(SearchHit const& hit);
 void NoteQuery(); void UpdateFavGlyph(std::wstring const& q);
 void SaveGeometry();
public:
 SearchWindow(Controller&);
 ~SearchWindow();
 void Show();
};
}
