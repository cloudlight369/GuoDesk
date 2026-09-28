#include "pch.h"
#include "App.xaml.h"
namespace guodesk { void RunTests(std::filesystem::path const&); }
static std::filesystem::path dataDirectory;
static bool desktopRequested=false;
static auto started=std::chrono::steady_clock::now();
namespace winrt::GuoDesk::implementation {
App::App() { InitializeComponent(); }
void App::OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&) {
 controller=std::make_unique<guodesk::Controller>(dataDirectory);
 controller->Start();
 if(desktopRequested)controller->ToggleDesktop();
 auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
 std::ofstream log(controller->store.Directory()/L"startup.log");log<<"Startup to windows created: "<<elapsed<<" ms\n";
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){try{winrt::init_apartment(winrt::apartment_type::single_threaded);int argc{};auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);bool test=false;std::filesystem::path report;for(int i=1;i<argc;++i){std::wstring arg=argv[i];if(arg==L"--self-test" && i+1<argc){test=true;report=argv[++i];}else if(arg==L"--data-dir"&&i+1<argc)dataDirectory=argv[++i];else if(arg==L"--desktop")desktopRequested=true;}LocalFree(argv);if(test){guodesk::RunTests(report);return 0;}winrt::Microsoft::UI::Xaml::Application::Start([](auto&&){winrt::make<winrt::GuoDesk::implementation::App>();});return 0;}catch(guodesk::already_running const&){return 0;}catch(winrt::hresult_error const& e){MessageBoxW(nullptr,e.message().c_str(),L"GuoDesk 启动失败",MB_OK|MB_ICONERROR);return 1;}catch(std::exception const& e){MessageBoxW(nullptr,winrt::to_hstring(e.what()).c_str(),L"GuoDesk",MB_OK|MB_ICONERROR);return 1;}}
