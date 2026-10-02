#pragma once
#include "Core.h"
namespace guodesk::ev {
bool Available();
std::vector<SearchHit> Query(std::wstring const& text,unsigned maxResults);
inline bool ShouldQuery(std::wstring const& text,bool enabled){return enabled&&text.size()>=2;}
std::vector<SearchHit> MergeHits(std::vector<SearchHit>& hits,std::vector<SearchHit> const& files);
}
