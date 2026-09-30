#pragma once
#include <initializer_list>
#include <string>
namespace guodesk::i18n {
void SetLanguage(std::wstring const& setting);
std::wstring CurrentLanguage();
std::wstring Tr(std::wstring const& key);
std::wstring TrF(std::wstring const& key,std::initializer_list<std::wstring> args);
std::wstring Fmt(std::wstring const& text,std::initializer_list<std::wstring> args);
}
