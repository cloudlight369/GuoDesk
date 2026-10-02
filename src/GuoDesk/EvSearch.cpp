#include "pch.h"
#include "EvSearch.h"
#define EVERYTHINGUSERAPI
#include "../../vendor/Everything/include/Everything.h"
#include "../../vendor/Everything/ipc/Everything_IPC.h"
namespace guodesk::ev {
bool Available(){return FindWindowW(EVERYTHING_IPC_WNDCLASSW,nullptr)!=nullptr;}
std::vector<SearchHit> Query(std::wstring const& text,unsigned maxResults){
 std::vector<SearchHit> out;
 if(!Available())return out;
 Everything_Reset();
 Everything_SetSearchW(text.c_str());
 Everything_SetRequestFlags(EVERYTHING_REQUEST_FILE_NAME|EVERYTHING_REQUEST_FULL_PATH_AND_FILE_NAME);
 Everything_SetMax(static_cast<DWORD>(maxResults));
 Everything_SetSort(EVERYTHING_SORT_NAME_ASCENDING);
 if(!Everything_QueryW(TRUE))return out;
 wchar_t full[4096]{};
 DWORD n=Everything_GetNumResults();
 for(DWORD i=0;i<n;i++){
  SearchHit h;h.kind=L"file";
  Everything_GetResultFullPathNameW(i,full,4096);h.path=full;
  LPCWSTR name=Everything_GetResultFileNameW(i);
  h.name=name&&*name?std::wstring(name):std::filesystem::path(h.path).filename().wstring();
  if(!h.path.empty())out.push_back(std::move(h));
 }
 return out;
}
std::vector<SearchHit> MergeHits(std::vector<SearchHit>& hits,std::vector<SearchHit> const& files){
 std::vector<SearchHit> added;
 for(auto const& f:files){
  if(f.path.empty())continue;
  bool dup=std::any_of(hits.begin(),hits.end(),[&](SearchHit const& h){return !h.path.empty()&&PathKey(h.path)==PathKey(f.path);});
  if(!dup){hits.push_back(f);added.push_back(f);}
 }
 return added;
}
}
