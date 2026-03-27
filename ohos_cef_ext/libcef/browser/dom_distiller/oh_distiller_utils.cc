/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "libcef/browser/dom_distiller/oh_distiller_utils.h"

#include <memory>
#include <string>
#include <utility>

#include "arkweb/chromium_ext/base/arkweb_report_statistics.h"
#include "arkweb/ohos_nweb_ex/overrides/cef/libcef/browser/alloy/alloy_browser_reader_mode_config.h"
#include "base/containers/lru_cache.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"

namespace oh_dom_distiller {

namespace {

// JSON field name constants
constexpr char kPageCount[] = "pageCount";
constexpr char kBookName[] = "bookName";
constexpr char kChapterName[] = "chapterName";
constexpr char kDescription[] = "description";
constexpr char kCoverUrl[] = "coverUrl";
constexpr char kAuthor[] = "author";
constexpr char kPageContent[] = "pageContent";
constexpr char kCategory[] = "category";
constexpr char kLastChapterName[] = "lastChapterName";
constexpr char kLastChapterUrl[] = "lastChapterUrl";
constexpr char kStartReadLink[] = "startReadLink";
constexpr char kFinishedStatus[] = "finishedStatus";
constexpr char kType[] = "type";
constexpr char kContent[] = "content";
constexpr char kCatalog[] = "catalog";
constexpr char kNextPageUrl[] = "nextPageUrl";
constexpr char kPrevPageUrl[] = "prevPageUrl";
constexpr char kCatalogPageUrl[] = "catalogPageUrl";
constexpr char kLatestChapters[] = "latestChapters";
constexpr char kAllChapters[] = "allChapters";
constexpr char kChapterUrl[] = "chapterUrl";
constexpr char kMorePageUrl[] = "morePageUrl";
constexpr char kPageName[] = "pageName";
constexpr char kPageUrl[] = "pageUrl";
constexpr char kSelected[] = "selected";
constexpr char kPageSelectList[] = "pageSelectList";
constexpr char kCatalogPagination[] = "catalogPagination";
constexpr char kTitle[] = "title";
constexpr char kTotalTime[] = "total_time";
constexpr char kDistillTime[] = "distill_time";
constexpr char kTimingInfo[] = "timingInfo";
constexpr char kCurrentPageUrl[] = "currentPageUrl";
constexpr char kResultList[] = "resultList";

// BECE field name constants
constexpr char kBECENovelDistillResult[] = "kernel_novel_distill_result";
constexpr char kBECEHost[] = "host";
constexpr char kBECEResultCode[] = "result_code";
constexpr char kBECEResultMessage[] = "result_message";
constexpr char kBECEPageType[] = "page_type";
constexpr char kBECELatestChaptersCount[] = "latest_chapters_count";
constexpr char kBECEAllChaptersCount[] = "all_chapters_count";
constexpr char kBECEHasNextUrl[] = "has_next_url";
constexpr char kBECEHasPrevUrl[] = "has_prev_url";
constexpr char kBECEHasCatalogUrl[] = "has_catalog_url";
constexpr char kBECEHasWholeUrl[] = "has_whole_url";
constexpr char kBECESelectUrlCount[] = "select_url_count";
constexpr char kBECEHasBookName[] = "has_book_name";
constexpr char kBECEHasBookImage[] = "has_book_image";
constexpr char kBECEHasBookAuthor[] = "has_book_author";
constexpr char kBECEDistillTime[] = "distill_time";
constexpr char kBECETotalTime[] = "total_time";
constexpr char kBECEDomDistillerJsVersion[] = "domdistillerjs_version";
constexpr char kHasTrue[] = "1";

static base::Value::Dict g_statistics_content;
static const int kRecentlyStatisticsUrlCount = 10;
static base::LRUCacheSet<GURL> g_recently_statistics_urls(
    kRecentlyStatisticsUrlCount);

void AddBECEContent(
    base::Value::Dict& statistics_content,
    std::initializer_list<std::pair<std::string, std::string>> contents,
    std::pair<bool, GURL> statistics = {false, GURL()}) {
  for (const auto& content : contents) {
    statistics_content.Set(content.first, content.second);
  }

  if (statistics.first && statistics.second.is_valid()
      && !statistics.second.is_empty()) {
    if (g_recently_statistics_urls.Get(statistics.second) ==
        g_recently_statistics_urls.end()) {
      g_recently_statistics_urls.Put(GURL(statistics.second));
      auto json = base::WriteJson(statistics_content);
      if (json) {
        base::ohos::OperationStatistics::Statistics(
            base::ohos::REGION_CHINA, base::ohos::PLATFORM_OPERATION_ANALYSIS,
            base::ohos::OperationStatistics::GROUP_BECE, kBECENovelDistillResult,
            base::ohos::OperationStatistics::DEFAULT_DATA_VERSION, json.value(),
            true, false, true, base::ohos::REPORT_DAILY);
      }
    }
    statistics_content.clear();
  }
}

bool FillResultFields(
    const dom_distiller::DistilledPageProto_HwReadExtendFields& flat,
    base::Value::Dict& dict) {
  dict.Set(kResultCode, 0);
  if (flat.has_distilled_result()) {
    const auto& res = flat.distilled_result();
    if (res.has_code()) {
      dict.Set(kResultCode, res.code());
      AddBECEContent(g_statistics_content,
                     {{kBECEResultCode, base::NumberToString(res.code())}});
    }
    if (res.has_message()) {
      dict.Set(kResultMessage, res.message());
      AddBECEContent(g_statistics_content,
                     {{kBECEResultMessage, res.message()}});
    }
    return res.has_code();
  }
  return false;
}

void FillBasicInfoFields(
    const dom_distiller::DistilledPageProto_HwReadExtendFields& flat,
    base::Value::Dict& dict) {
  if (flat.has_book_name()) {
    dict.Set(kBookName, flat.book_name());
    AddBECEContent(g_statistics_content, {{kBECEHasBookName, "1"}});
  }
  if (flat.has_chapter_name()) {
    dict.Set(kChapterName, flat.chapter_name());
  }
  if (flat.has_description()) {
    dict.Set(kDescription, flat.description());
  }
  if (flat.has_img_url()) {
    dict.Set(kCoverUrl, flat.img_url());
    AddBECEContent(g_statistics_content, {{kBECEHasBookImage, "1"}});
  }
  if (flat.has_author()) {
    dict.Set(kAuthor, flat.author());
    AddBECEContent(g_statistics_content, {{kBECEHasBookAuthor, "1"}});
  }
  if (flat.has_content()) {
    dict.Set(kPageContent, flat.content());
  }
  if (flat.has_category()) {
    dict.Set(kCategory, flat.category());
  }
  if (flat.has_last_chapter_name()) {
    dict.Set(kLastChapterName, flat.last_chapter_name());
  }
  if (flat.has_last_chapter_url()) {
    dict.Set(kLastChapterUrl, flat.last_chapter_url());
  }
  if (flat.has_start_read_link()) {
    dict.Set(kStartReadLink, flat.start_read_link());
  }
  if (flat.has_finished_status()) {
    dict.Set(kFinishedStatus, flat.finished_status());
  }
}

void FillContentInfoFields(
    const dom_distiller::DistilledPageProto_HwReadExtendFields& flat,
    bool has_result,
    base::Value::Dict& dict) {
  if (flat.has_content_info()) {
    dict.Set(kType, kContent);
    AddBECEContent(g_statistics_content, {{kBECEPageType, kContent}});
    const auto& info = flat.content_info();
    if (info.has_next_page()) {
      dict.Set(kNextPageUrl, info.next_page());
      AddBECEContent(g_statistics_content, {{kBECEHasNextUrl, "1"}});
    }
    if (info.has_prev_page()) {
      dict.Set(kPrevPageUrl, info.prev_page());
      AddBECEContent(g_statistics_content, {{kBECEHasPrevUrl, "1"}});
    }
    if (info.has_catalog_page()) {
      dict.Set(kCatalogPageUrl, info.catalog_page());
      AddBECEContent(g_statistics_content, {{kBECEHasCatalogUrl, "1"}});
    }
  }
  if (!has_result && ((flat.has_content_info() && !flat.has_content()) ||
                      (flat.has_content() && flat.content().empty()))) {
    dict.Set(kResultCode, kNoContentErrorCode);
    dict.Set(kResultMessage, kErrorMsgNoContent);
    AddBECEContent(g_statistics_content,
                   {{kBECEResultCode, base::NumberToString(kNoContentErrorCode)},
                    {kBECEResultMessage, kErrorMsgNoContent}});
  }
}

bool FillPaginationInfo(const dom_distiller::DistilledPageProto_HwReadExtendFields_CatalogPageInfo& cat,
  base::Value::Dict& dict) {
  bool has_pagination = false;
  if (!cat.has_pagination_info()) {
    LOG(WARNING) << __func__ << "[Distiller] no pagination_info";
    return has_pagination;
  }
  base::Value::Dict pag;
  const auto& p = cat.pagination_info();
  if (p.has_next_catalog_page()) {
    pag.Set(kNextPageUrl, p.next_catalog_page());
    AddBECEContent(g_statistics_content, {{kBECEHasNextUrl, "1"}});
    has_pagination = !p.next_catalog_page().empty();
  }
  if (p.has_prev_catalog_page()) {
    pag.Set(kPrevPageUrl, p.prev_catalog_page());
    AddBECEContent(g_statistics_content, {{kBECEHasPrevUrl, "1"}});
    has_pagination = !p.prev_catalog_page().empty();
  }
  if (p.has_whole_catalog_page()) {
    pag.Set(kMorePageUrl, p.whole_catalog_page());
    AddBECEContent(g_statistics_content, {{kBECEHasWholeUrl, "1"}});
    has_pagination = !p.whole_catalog_page().empty();
  }

  base::Value::List pages;
  for (const auto& cp : p.all_catalog_pages()) {
    base::Value::Dict pi;
    pi.Set(kPageName, cp.page_name());
    pi.Set(kPageUrl, cp.page_url());
    if (cp.has_selected() && cp.selected()) {
      pi.Set(kSelected, cp.selected());
    }
    pages.Append(std::move(pi));
    has_pagination = true;
  }
  if (!pages.empty()) {
    pag.Set(kPageSelectList, std::move(pages));
  }
  dict.Set(kCatalogPagination, std::move(pag));
  AddBECEContent(g_statistics_content,
                 {{kBECESelectUrlCount, base::NumberToString(p.all_catalog_pages_size())}});
  return has_pagination;
}

void FillCatalogInfoFields(
    const dom_distiller::DistilledPageProto_HwReadExtendFields& flat,
    bool has_result,
    base::Value::Dict& dict) {
  if (!flat.has_catalog_info()) {
    LOG(WARNING) << __func__ << "[Distiller] no catalog_info";
    return;
  }
  dict.Set(kType, kCatalog);
  const auto& cat = flat.catalog_info();

  base::Value::List latest;
  base::Value::List all;
  for (const auto& ch : cat.latest_chapters()) {
    base::Value::Dict ci;
    ci.Set(kChapterName, ch.chapter_name());
    ci.Set(kChapterUrl, ch.chapter_url());
    latest.Append(std::move(ci));
  }
  if (!latest.empty()) {
    dict.Set(kLatestChapters, std::move(latest));
  }

  for (const auto& ch : cat.all_chapters()) {
    base::Value::Dict ci;
    ci.Set(kChapterName, ch.chapter_name());
    ci.Set(kChapterUrl, ch.chapter_url());
    all.Append(std::move(ci));
  }
  if (!all.empty()) {
    dict.Set(kAllChapters, std::move(all));
  }

  AddBECEContent(g_statistics_content,
                 {{kBECEPageType, kCatalog},
                  {kBECELatestChaptersCount, base::NumberToString(cat.latest_chapters_size())},
                  {kBECEAllChaptersCount, base::NumberToString(cat.all_chapters_size())}});

  bool has_pagination = FillPaginationInfo(cat, dict);
  if (!has_result && cat.all_chapters_size() == 0 && !has_pagination) {
    dict.Set(kResultCode, kNoChaptersErrorCode);
    dict.Set(kResultMessage, kErrorMsgNoChapters);
    AddBECEContent(g_statistics_content,
                   {{kBECEResultCode, base::NumberToString(kNoChaptersErrorCode)},
                    {kBECEResultMessage, kErrorMsgNoChapters}});
  }
}

void FillTimingInfoFields(
    const dom_distiller::DistilledPageProto_HwReadExtendFields& flat,
    base::Value::Dict& dict) {
  if (flat.has_timing_info()) {
    const auto& t = flat.timing_info();
    base::Value::Dict timing;
    if (t.has_total_time()) {
      std::string total_time = base::StringPrintf("%.2f", t.total_time());
      timing.Set(kTotalTime, total_time);
      AddBECEContent(g_statistics_content, {{kBECETotalTime, total_time}});
    }
    if (t.has_distill_time()) {
      std::string distill_time = base::StringPrintf("%.2f", t.distill_time());
      timing.Set(kDistillTime, distill_time);
      AddBECEContent(g_statistics_content, {{kBECEDistillTime, distill_time}});
    }
    dict.Set(kTimingInfo, std::move(timing));
  }
}

bool BuildHwDistillerResultJson(const dom_distiller::DistilledPageProto& page,
                                base::Value::Dict& result_dict) {
  if (!page.has_hw_distiller_result_json()) {
    return false;
  }
  if (page.hw_distiller_result_json().empty()) {
    return false;
  }
  LOG(DEBUG) << __func__ << " [Distiller] js resutl hw_distiller_result_json size: "
             << page.hw_distiller_result_json().length();
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
                                       page.hw_distiller_result_json());
  if (!parsed_json.has_value()) {
    LOG(WARNING) << __func__ << " [Distiller] Error parsing hw_distiller_result_json: "
                 << parsed_json.error().message;
    return false;
  }

  base::Value value = std::move(*parsed_json);
  if (!value.is_dict()) {
    return false;
  }
  result_dict = std::move(value.GetDict());
  return true;
}

void BuildFlagInfo(const base::Value::Dict& result_dict) {
  if (const auto* book_name = result_dict.FindString(kBookName)) {
    AddBECEContent(g_statistics_content, {{kBECEHasBookName, kHasTrue}});
  }

  if (const auto* cover_url = result_dict.FindString(kCoverUrl)) {
    if (!cover_url->empty()) {
      AddBECEContent(g_statistics_content, {{kBECEHasBookImage, kHasTrue}});
    }
  }

  if (const auto* author = result_dict.FindString(kAuthor)) {
    if (!author->empty()) {
      AddBECEContent(g_statistics_content, {{kBECEHasBookAuthor, kHasTrue}});
    }
  }

  if (const auto* next_page = result_dict.FindString(kNextPageUrl)) {
    if (!next_page->empty()) {
      AddBECEContent(g_statistics_content, {{kBECEHasNextUrl, kHasTrue}});
    }
  }

  if (const auto* prev_page = result_dict.FindString(kPrevPageUrl)) {
    if (!prev_page->empty()) {
      AddBECEContent(g_statistics_content, {{kBECEHasPrevUrl, kHasTrue}});
    }
  }

  if (const auto* catalog_page = result_dict.FindString(kCatalogPageUrl)) {
    if (!catalog_page->empty()) {
      AddBECEContent(g_statistics_content, {{kBECEHasCatalogUrl, kHasTrue}});
    }
  }
}

void BuildChaptersInfo(const base::Value::Dict& result_dict) {
  if (const auto* latest_chapters = result_dict.FindList(kLatestChapters)) {
    int latest_chapters_count = 0;
    if (!latest_chapters->empty()) {
      latest_chapters_count = latest_chapters->size();
    }
    AddBECEContent(g_statistics_content,
                   {{kBECELatestChaptersCount, base::NumberToString(latest_chapters_count)}});
  }

  if (const auto* all_chapters = result_dict.FindList(kAllChapters)) {
    if (!all_chapters->empty()) {
      AddBECEContent(g_statistics_content,
                     {{kBECEAllChaptersCount, base::NumberToString(all_chapters->size())}});
    }
  }
}

void BuildPaginationInfo(const base::Value::Dict& result_dict) {
  if (const auto* pagination_info = result_dict.FindDict(kCatalogPagination)) {
    auto* next_catalog_page = pagination_info->FindString(kNextPageUrl);
    if (next_catalog_page && !next_catalog_page->empty()) {
      AddBECEContent(g_statistics_content, {{kBECEHasNextUrl, kHasTrue}});
    }

    auto* prev_catalog_page = pagination_info->FindString(kPrevPageUrl);
    if (prev_catalog_page && !prev_catalog_page->empty()) {
      AddBECEContent(g_statistics_content, {{kBECEHasPrevUrl, kHasTrue}});
    }

    auto* more_catalog_page = pagination_info->FindString(kMorePageUrl);
    if (more_catalog_page && !more_catalog_page->empty()) {
      AddBECEContent(g_statistics_content, {{kBECEHasWholeUrl, kHasTrue}});
    }

    auto* page_select_list = pagination_info->FindList(kPageSelectList);
    int page_select_list_count = 0;
    if (page_select_list && page_select_list->size() != 0) {
      page_select_list_count = page_select_list->size();
    }
    AddBECEContent(g_statistics_content,
                   {{kBECESelectUrlCount, base::NumberToString(page_select_list_count)}});
  }
}

void ReportDistilledResult(base::Value::Dict& dict,
                           const dom_distiller::DistilledPageProto& page) {
  AddBECEContent(g_statistics_content, {{kBECEHost, GURL(page.url()).host()}});
  int code = 0;
  if (std::optional<int> result_code = dict.FindInt(kResultCode)) {
    code = *result_code;
  } else {
    dict.Set(kResultCode, 0);
  }
  if (code != 0) {
    AddBECEContent(g_statistics_content, {{kBECEResultCode, base::NumberToString(code)}});
    const auto* result_message = dict.FindString(kResultMessage);
    if (result_message && !result_message->empty()) {
      AddBECEContent(g_statistics_content, {{kBECEResultMessage, *result_message}});
    }
  }
  if (const auto* page_type = dict.FindString(kType)) {
    if (!page_type->empty()) {
      AddBECEContent(g_statistics_content, {{kBECEPageType, *page_type}});
    }
  }
  BuildFlagInfo(dict);
  BuildChaptersInfo(dict);
  BuildPaginationInfo(dict);
  AddBECEContent(g_statistics_content,
                 {{kBECEDomDistillerJsVersion, base::NumberToString(
                        nweb_ex::AlloyBrowserReaderModeConfig::GetInstance()->GetDomDistillerJsVersion())}});
  AddBECEContent(g_statistics_content, {}, {true, GURL(page.url())});
}

void LogDistillResult(const base::Value::Dict& dict) {
  const int result_code = dict.FindInt(kResultCode).value_or(1);
  auto* result_message = dict.FindString(kResultMessage);
  auto* next_page_url = dict.FindString(kNextPageUrl);
  auto* prev_page_url = dict.FindString(kPrevPageUrl);
  auto* catalog_page_url = dict.FindString(kCatalogPageUrl);
  auto* content = dict.FindString(kPageContent);
  auto* latest_chapters = dict.FindList(kLatestChapters);
  auto* all_chapters = dict.FindList(kAllChapters);
  auto* type = dict.FindString(kType);

  std::string json;
  if (auto* timing_info = dict.FindDict(kTimingInfo)) {
    base::JSONWriter::Write(*timing_info, &json);
  }

  std::string pagination_str;
  if (auto* pagination = dict.FindDict(kCatalogPagination)) {
    auto* next_page = pagination->FindString(kNextPageUrl);
    auto* prev_page = pagination->FindString(kPrevPageUrl);
    auto* more_page = pagination->FindString(kMorePageUrl);
    auto* page_select = pagination->FindList(kPageSelectList);
    pagination_str = base::StringPrintf(
        "%d %d %d %zu", (next_page && !next_page->empty()),
        (prev_page && !prev_page->empty()), (more_page && !more_page->empty()),
        (page_select ? page_select->size() : 0));
  }

  LOG(INFO) << "[Distiller] DistillResult type:" << (type ? *type : "")
            << " resultCode:" << result_code << " "
            << (result_message ? *result_message : "")
            << " contents:" << (content ? (int)content->size() : -1) << " "
            << (next_page_url ? !next_page_url->empty() : false) << " "
            << (prev_page_url ? !prev_page_url->empty() : false) << " "
            << (catalog_page_url ? !catalog_page_url->empty() : false)
            << " chapters:"
            << (latest_chapters ? (int)latest_chapters->size() : -1) << " "
            << (all_chapters ? (int)all_chapters->size() : -1) << " "
            << pagination_str << " timing_info:" << json;
}

base::Value::Dict BuildPageContent(
    const dom_distiller::DistilledPageProto& page) {
  base::Value::Dict dict;
  if (BuildHwDistillerResultJson(page, dict)) {
    FillTimingInfoFields(page.flat_fields(), dict);
    ReportDistilledResult(dict, page);
  } else {
    LOG(DEBUG) << __func__ << " [Distiller] hw_distiller_result_json empty or invalid,"
               << " build result maintaining the previous logic.";
    const auto& flat = page.flat_fields();
    bool has_result_code = FillResultFields(flat, dict);
    FillBasicInfoFields(flat, dict);
    FillContentInfoFields(flat, has_result_code, dict);
    FillCatalogInfoFields(flat, has_result_code, dict);
    FillTimingInfoFields(flat, dict);

    if (page.has_title()) {
      dict.Set(kTitle, page.title());
    }
    dict.Set(kCurrentPageUrl, page.url());
    AddBECEContent(g_statistics_content, {{kBECEHost, GURL(page.url()).host()}});
    AddBECEContent(g_statistics_content, {}, {true, GURL(page.url())});
  }
  LogDistillResult(dict);
  return dict;
}

base::Value::List BuildDistilledPageList(
    const dom_distiller::DistilledArticleProto* proto) {
  base::Value::List list;
  for (size_t i = 0; i < static_cast<size_t>(proto->pages_size()); ++i) {
    list.Append(BuildPageContent(proto->pages(i)));
  }
  return list;
}

}  // namespace

std::unique_ptr<std::string> FormatDistilledArticleProtoToJsonData(
    const dom_distiller::DistilledArticleProto* article_proto,
    const GURL& url) {
  AddBECEContent(g_statistics_content, {{kBECEHost, url.host()}});

  auto result = std::make_unique<std::string>();
  base::Value::Dict json;

  if (!article_proto || article_proto->pages_size() == 0) {
    json.Set(kPageCount, 0);
    json.Set(kResultCode, kNoPageErrorCode);
    json.Set(kResultMessage, kErrorMsgNoPage);
    AddBECEContent(g_statistics_content,
                   {{kBECEResultCode, base::NumberToString(kNoPageErrorCode)},
                    {kBECEResultMessage, kErrorMsgNoPage}},
                   {true, url});
    base::JSONWriter::Write(std::move(json), result.get());
    LOG(INFO) << "[Distiller] DistillResult no pages";
    return result;
  }

  json.Set(kPageCount, article_proto->pages_size());
  base::Value::List page_list = BuildDistilledPageList(article_proto);
  if (!page_list.empty()) {
    json.Set(kResultList, std::move(page_list));
  }

  base::JSONWriter::Write(std::move(json), result.get());
  return result;
}

}  // namespace oh_dom_distiller
