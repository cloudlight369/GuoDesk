#pragma once
#include <string>
namespace guodesk::webdav {
struct UrlParts { std::wstring scheme, host, path; unsigned port=0; };
bool ParseUrl(std::wstring const& url,UrlParts& out);
std::wstring JoinUrl(std::wstring const& base,std::wstring const& name);
std::string Base64(std::string const& bytes);
std::wstring ProtectSecret(std::wstring const& plain);
std::wstring UnprotectSecret(std::wstring const& blob);
struct Result { bool ok=false; unsigned status=0; std::string body; };
Result Request(std::wstring const& method,std::wstring const& url,std::wstring const& user,std::wstring const& pass,std::string const& body=std::string());
bool UploadText(std::wstring const& url,std::wstring const& user,std::wstring const& pass,std::string const& content);
bool DownloadText(std::wstring const& url,std::wstring const& user,std::wstring const& pass,std::string& content);
}
