#pragma once
#include "Core.h"
namespace guodesk {
class Controller;
class WeatherWindow {
 Controller& owner; bool closing=false;
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
 winrt::Microsoft::UI::Xaml::Controls::Grid root{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock city{nullptr},icon{nullptr},temp{nullptr},desc{nullptr},extra{nullptr},updated{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::StackPanel days{nullptr},hours{nullptr};
 winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer refresh{nullptr},dragTimer{nullptr};
 std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
 double lat=0,lon=0; std::wstring cityName; bool resolved=false; int skinMode=0;
 winrt::Microsoft::UI::Xaml::Media::Brush ThemeBrush(wchar_t const* key,winrt::Windows::UI::Color fallback);
 void ResolveCoords(); void Fetch(); void Render(WeatherNow const& w); void SetStatus(std::wstring const& text); void ApplySkin(int code,bool night);
 void SaveGeometry();
public:
 WeatherWindow(Controller&);
 ~WeatherWindow();
 void Show(); void Reload();
};
std::string HttpGetJson(std::wstring const& host,std::wstring const& path,bool secure);
std::wstring UrlParam(std::wstring const& value);
}
