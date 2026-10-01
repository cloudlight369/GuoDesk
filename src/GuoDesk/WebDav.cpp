#include "pch.h"
#include "WebDav.h"
#include <winhttp.h>
#include <wincrypt.h>
namespace guodesk::webdav {
namespace {
std::wstring Lower(std::wstring v){for(auto& c:v)c=(wchar_t)towlower(c);return v;}
std::string ToUtf8(std::wstring const& text){return winrt::to_string(text);}
std::wstring ToWide(std::string const& bytes){return std::wstring(winrt::to_hstring(bytes));}
}
bool ParseUrl(std::wstring const& url,UrlParts& out){
 out=UrlParts{};
 auto pos=url.find(L"://");if(pos==std::wstring::npos)return false;
 out.scheme=Lower(url.substr(0,pos));if(out.scheme!=L"http"&&out.scheme!=L"https")return false;
 auto rest=url.substr(pos+3);if(rest.empty())return false;
 auto slash=rest.find(L'/');
 auto hostport=slash==std::wstring::npos?rest:rest.substr(0,slash);
 out.path=slash==std::wstring::npos?L"/":rest.substr(slash);
 auto colon=hostport.rfind(L':');
 if(colon!=std::wstring::npos){auto p=hostport.substr(colon+1);if(p.empty()||p.find_first_not_of(L"0123456789")!=std::wstring::npos)return false;out.port=(unsigned)_wtoi(p.c_str());if(out.port==0||out.port>65535)return false;out.host=hostport.substr(0,colon);}
 else{out.host=hostport;out.port=out.scheme==L"https"?443u:80u;}
 if(out.host.empty())return false;
 return true;
}
std::wstring JoinUrl(std::wstring const& base,std::wstring const& name){std::wstring b=base;while(!b.empty()&&b.back()==L'/')b.pop_back();return b+L"/"+name;}
std::string Base64(std::string const& bytes){
 if(bytes.empty())return{};
 DWORD size=0;
 if(!CryptBinaryToStringA(reinterpret_cast<BYTE const*>(bytes.data()),(DWORD)bytes.size(),CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,nullptr,&size))return{};
 std::string out(size,'\0');
 if(!CryptBinaryToStringA(reinterpret_cast<BYTE const*>(bytes.data()),(DWORD)bytes.size(),CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,&out[0],&size))return{};
 while(!out.empty()&&(out.back()=='\0'||out.back()=='\r'||out.back()=='\n'))out.pop_back();
 return out;
}
std::wstring ProtectSecret(std::wstring const& plain){
 if(plain.empty())return{};
 DATA_BLOB in{};in.pbData=reinterpret_cast<BYTE*>(const_cast<wchar_t*>(plain.data()));in.cbData=(DWORD)plain.size()*sizeof(wchar_t);
 DATA_BLOB out{};
 if(!CryptProtectData(&in,L"GuoDesk",nullptr,nullptr,nullptr,0,&out))return{};
 DWORD size=0;std::string blob;
 if(CryptBinaryToStringA(out.pbData,out.cbData,CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,nullptr,&size)){blob.assign(size,'\0');if(CryptBinaryToStringA(out.pbData,out.cbData,CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,&blob[0],&size)){while(!blob.empty()&&(blob.back()=='\0'||blob.back()=='\r'||blob.back()=='\n'))blob.pop_back();}}
 LocalFree(out.pbData);
 return ToWide(blob);
}
std::wstring UnprotectSecret(std::wstring const& blob){
 if(blob.empty())return{};
 DWORD size=0;
 if(!CryptStringToBinaryW(blob.c_str(),(DWORD)blob.size(),CRYPT_STRING_BASE64,nullptr,&size,nullptr,nullptr))return{};
 std::string bytes(size,'\0');
 if(!CryptStringToBinaryW(blob.c_str(),(DWORD)blob.size(),CRYPT_STRING_BASE64,reinterpret_cast<BYTE*>(&bytes[0]),&size,nullptr,nullptr))return{};
 DATA_BLOB in{};in.pbData=reinterpret_cast<BYTE*>(&bytes[0]);in.cbData=size;
 DATA_BLOB out{};
 if(!CryptUnprotectData(&in,nullptr,nullptr,nullptr,nullptr,0,&out))return{};
 std::wstring plain(reinterpret_cast<wchar_t*>(out.pbData),out.cbData/sizeof(wchar_t));
 LocalFree(out.pbData);
 return plain;
}
Result Request(std::wstring const& method,std::wstring const& url,std::wstring const& user,std::wstring const& pass,std::string const& body){
 Result r;UrlParts up;
 if(!ParseUrl(url,up))return r;
 HINTERNET session=WinHttpOpen(L"GuoDesk/1.1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
 if(!session)return r;
 HINTERNET connect=WinHttpConnect(session,up.host.c_str(),(INTERNET_PORT)up.port,0);
 HINTERNET request=connect?WinHttpOpenRequest(connect,method.c_str(),up.path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,up.scheme==L"https"?WINHTTP_FLAG_SECURE:0):nullptr;
 do{
  if(!request)break;
  DWORD t=10000;WinHttpSetOption(request,WINHTTP_OPTION_CONNECT_TIMEOUT,&t,sizeof(t));WinHttpSetOption(request,WINHTTP_OPTION_SEND_TIMEOUT,&t,sizeof(t));WinHttpSetOption(request,WINHTTP_OPTION_RECEIVE_TIMEOUT,&t,sizeof(t));
  std::wstring headers;
  if(!user.empty()||!pass.empty())headers=L"Authorization: Basic "+ToWide(Base64(ToUtf8(user+L":"+pass)))+L"\r\n";
  if(!body.empty())headers+=L"Content-Type: application/json\r\n";
  BOOL sent=WinHttpSendRequest(request,headers.empty()?WINHTTP_NO_ADDITIONAL_HEADERS:headers.c_str(),(DWORD)headers.size(),(LPVOID)(body.empty()?nullptr:body.data()),(DWORD)body.size(),(DWORD)body.size(),0);
  if(!sent&&GetLastError()==ERROR_WINHTTP_SECURE_FAILURE){
   DWORD flags=SECURITY_FLAG_IGNORE_UNKNOWN_CA|SECURITY_FLAG_IGNORE_CERT_CN_INVALID|SECURITY_FLAG_IGNORE_CERT_DATE_INVALID|SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
   WinHttpSetOption(request,WINHTTP_OPTION_SECURITY_FLAGS,&flags,sizeof(flags));
   sent=WinHttpSendRequest(request,headers.empty()?WINHTTP_NO_ADDITIONAL_HEADERS:headers.c_str(),(DWORD)headers.size(),(LPVOID)(body.empty()?nullptr:body.data()),(DWORD)body.size(),(DWORD)body.size(),0);
  }
  if(!sent||!WinHttpReceiveResponse(request,nullptr))break;
  DWORD status=0,ssize=sizeof(status);
  if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&ssize,WINHTTP_NO_HEADER_INDEX))break;
  r.status=status;
  std::string data;char buf[16384];DWORD read=0;
  while(WinHttpReadData(request,buf,sizeof(buf),&read)&&read>0)data.append(buf,read);
  r.body=std::move(data);
  r.ok=status>=200&&status<300;
 }while(false);
 if(request)WinHttpCloseHandle(request);
 if(connect)WinHttpCloseHandle(connect);
 WinHttpCloseHandle(session);
 return r;
}
bool UploadText(std::wstring const& url,std::wstring const& user,std::wstring const& pass,std::string const& content){
 auto r=Request(L"PUT",url,user,pass,content);
 if(r.ok)return true;
 auto slash=url.find_last_of(L'/');auto schemeend=url.find(L"//");
 if(slash!=std::wstring::npos&&(schemeend==std::wstring::npos||slash>schemeend+1)){
  Request(L"MKCOL",url.substr(0,slash),user,pass);
  r=Request(L"PUT",url,user,pass,content);
 }
 return r.ok;
}
bool DownloadText(std::wstring const& url,std::wstring const& user,std::wstring const& pass,std::string& content){
 auto r=Request(L"GET",url,user,pass);
 if(!r.ok)return false;
 content=std::move(r.body);
 return true;
}
}
