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
#include <hilog/log.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#include "chrome/common/extensions/api/downloads.h"
#include "ohos_nweb/src/cef_delegate/nweb_extension_downloads_cef_delegate.h"
#include "third_party/bounds_checking_function/include/securec.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {
namespace {
namespace downloads = api::downloads;

#define NWEB_DOWNLOADS_SAFE_FREE(ptr) \
  do {                                \
    if (ptr) {                        \
      free(ptr);                      \
      ptr = nullptr;                  \
    }                                 \
  } while (0)

#define EXTENSION_OP_PARAMS_GET_DOUBLE(c, v, result)    \
  if (c) {                                              \
    (v) = static_cast<double*>(malloc(sizeof(double))); \
    if (!(v)) {                                         \
      LOG(INFO) << "malloc double file: " << (#v);      \
      (result) = false;                                 \
    } else {                                            \
      *(v) = (c).value();                               \
    }                                                   \
  }

#define EXTENSION_OP_PARAMS_GET_STRING(c, v, result) \
  if (c) {                                           \
    (v) = strdup((c).value().c_str());               \
    if (!(v)) {                                      \
      LOG(INFO) << "malloc string file: " << (#v);   \
      (result) = false;                              \
    }                                                \
  }

#define EXTENSION_OP_PARAMS_GET_BOOL(c, v, result)  \
  if (c) {                                          \
    (v) = static_cast<bool*>(malloc(sizeof(bool))); \
    if (!(v)) {                                     \
      LOG(INFO) << "malloc bool file: " << (#v);    \
      (result) = false;                             \
    } else {                                        \
      *(v) = (c).value();                           \
    }                                               \
  }

#define EXTENSION_OP_PARAMS_GET_INT(c, v, result) \
  if (c) {                                        \
    (v) = static_cast<int*>(malloc(sizeof(int))); \
    if (!(v)) {                                   \
      LOG(INFO) << "malloc int file: " << (#v);   \
      (result) = false;                           \
    } else {                                      \
      *(v) = (c).value();                         \
    }                                             \
  }

#define OP_PARAMS_ALLOCATE_MAMORY_VECTOR_STR(c, v, result)                 \
  if ((c) && (result)) {                                                   \
    strList = *(c);                                                        \
    (v) = static_cast<ExDownloadsVectorCharType*>(                         \
        malloc(sizeof(ExDownloadsVectorCharType)));                        \
    (v)->count = static_cast<uint32_t>(strList.size());                    \
    (v)->strs = static_cast<char**>(malloc(((v)->count) * sizeof(char*))); \
    if (!(v)->strs) {                                                      \
      LOG(ERROR) << "vector strs malloc failed." << (#v);                  \
      (result) = false;                                                    \
    }                                                                      \
  }

#define OP_PARAMS_COPY_VECTOR_STRINGS(v, result)     \
  if (v) {                                           \
    for (uint32_t i = 0; i < (v)->count; ++i) {      \
      (v)->strs[i] = strdup(strList[i].c_str());     \
      if (!(v)->strs[i]) {                           \
        LOG(INFO) << "vector strdup file: " << (#v); \
        (result) = false;                            \
      }                                              \
    }                                                \
  }

#define EXTENSION_PARAMS_GET_ENUMINT(c, v, result) \
  (v) = static_cast<int*>(malloc(sizeof(int)));    \
  if (!(v)) {                                      \
    LOG(INFO) << "malloc enum int file: " << (#v); \
    (result) = false;                              \
  } else {                                         \
    *(v) = static_cast<int>(c);                    \
  }

#define EXTENSION_PARAMS_GET_BOOL(c, v, result)     \
  do {                                              \
    (v) = static_cast<bool*>(malloc(sizeof(bool))); \
    if (!(v)) {                                     \
      LOG(INFO) << "malloc bool file: " << (#v);    \
      (result) = false;                             \
    } else {                                        \
      *(v) = (c);                                   \
    }                                               \
  } while (0)

#define EXTENSION_PARAMS_GET_STRING(c, v, result)  \
  do {                                             \
    (v) = strdup((c).c_str());                     \
    if (!(v)) {                                    \
      LOG(INFO) << "malloc string file: " << (#v); \
      (result) = false;                            \
    }                                              \
  } while (0)

#define EXTENSION_PARAMS_GET_INT(c, v, result)    \
  do {                                            \
    (v) = static_cast<int*>(malloc(sizeof(int))); \
    if (!(v)) {                                   \
      LOG(INFO) << "malloc int file: " << (#v);   \
      (result) = false;                           \
    } else {                                      \
      *(v) = (c);                                 \
    }                                             \
  } while (0)

enum DownloadsFunctionName {
  DOWNLOADS_FUNCTION_DOWNLOAD = 0,
  DOWNLOADS_FUNCTION_SEARCH = 1,
  DOWNLOADS_FUNCTION_PAUSE = 2,
  DOWNLOADS_FUNCTION_RESUME = 3,
  DOWNLOADS_FUNCTION_CANCEL = 4,
  DOWNLOADS_FUNCTION_ERASE = 5,
  // 6 unused
  DOWNLOADS_FUNCTION_ACCEPT_DANGER = 7,
  DOWNLOADS_FUNCTION_SHOW = 8,
  DOWNLOADS_FUNCTION_DRAG = 9,
  DOWNLOADS_FUNCTION_GET_FILE_ICON = 10,
  DOWNLOADS_FUNCTION_OPEN = 11,
  DOWNLOADS_FUNCTION_REMOVE_FILE = 12,
  DOWNLOADS_FUNCTION_SHOW_DEFAULT_FOLDER = 13,
  DOWNLOADS_FUNCTION_SET_SHELF_ENABLED = 14,
  DOWNLOADS_FUNCTION_DETERMINE_FILENAME = 15,
  DOWNLOADS_FUNCTION_SET_UI_OPTIONS = 16,
  // Insert new values here, not at the beginning.
  DOWNLOADS_FUNCTION_LAST
};

/* queryinfo */
bool GetEraseDownloadQueryInfoParams(
    std::optional<downloads::Erase::Params>& params,
    ExDownloadsQueryInfo* queryInfo) {
  std::vector<std::string> strList;
  bool result = true;
  EXTENSION_OP_PARAMS_GET_DOUBLE(params->query.bytes_received,
                                 queryInfo->bytesReceived, result);
  EXTENSION_PARAMS_GET_ENUMINT(params->query.danger, queryInfo->danger, result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.end_time, queryInfo->endTime,
                                 result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.ended_after,
                                 queryInfo->endedAfter, result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.ended_before,
                                 queryInfo->endedBefore, result);
  EXTENSION_PARAMS_GET_ENUMINT(params->query.error, queryInfo->error, result);
  EXTENSION_OP_PARAMS_GET_BOOL(params->query.exists, queryInfo->exists, result);
  EXTENSION_OP_PARAMS_GET_DOUBLE(params->query.file_size, queryInfo->fileSize,
                                 result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.filename, queryInfo->filename,
                                 result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.filename_regex,
                                 queryInfo->filenameRegex, result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.final_url, queryInfo->finalUrl,
                                 result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.final_url_regex,
                                 queryInfo->finalUrlRegex, result);
  EXTENSION_OP_PARAMS_GET_INT(params->query.id, queryInfo->id, result);
  EXTENSION_OP_PARAMS_GET_INT(params->query.limit, queryInfo->limit, result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.mime, queryInfo->mime, result);

  OP_PARAMS_ALLOCATE_MAMORY_VECTOR_STR(params->query.order_by,
                                       queryInfo->orderBy, result);
  OP_PARAMS_COPY_VECTOR_STRINGS(queryInfo->orderBy, result);

  EXTENSION_OP_PARAMS_GET_BOOL(params->query.paused, queryInfo->paused, result);

  OP_PARAMS_ALLOCATE_MAMORY_VECTOR_STR(params->query.order_by, queryInfo->query,
                                       result);
  OP_PARAMS_COPY_VECTOR_STRINGS(queryInfo->query, result);

  EXTENSION_OP_PARAMS_GET_STRING(params->query.start_time, queryInfo->startTime,
                                 result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.started_after,
                                 queryInfo->startedAfter, result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.started_before,
                                 queryInfo->startedBefore, result);
  EXTENSION_PARAMS_GET_ENUMINT(params->query.state, queryInfo->state, result);
  EXTENSION_OP_PARAMS_GET_DOUBLE(params->query.total_bytes,
                                 queryInfo->totalBytes, result);
  EXTENSION_OP_PARAMS_GET_DOUBLE(params->query.total_bytes_greater,
                                 queryInfo->totalBytesGreater, result);
  EXTENSION_OP_PARAMS_GET_DOUBLE(params->query.total_bytes_less,
                                 queryInfo->totalBytesLess, result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.url, queryInfo->url, result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.url_regex, queryInfo->urlRegex,
                                 result);

  return result;
}

bool GetSearchDownloadQueryInfoParams(
    std::optional<downloads::Search::Params>& params,
    ExDownloadsQueryInfo* queryInfo) {
  std::vector<std::string> strList;
  bool result = true;
  EXTENSION_OP_PARAMS_GET_DOUBLE(params->query.bytes_received,
                                 queryInfo->bytesReceived, result);
  EXTENSION_PARAMS_GET_ENUMINT(params->query.danger, queryInfo->danger, result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.end_time, queryInfo->endTime,
                                 result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.ended_after,
                                 queryInfo->endedAfter, result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.ended_before,
                                 queryInfo->endedBefore, result);
  EXTENSION_PARAMS_GET_ENUMINT(params->query.error, queryInfo->error, result);
  EXTENSION_OP_PARAMS_GET_BOOL(params->query.exists, queryInfo->exists, result);
  EXTENSION_OP_PARAMS_GET_DOUBLE(params->query.file_size, queryInfo->fileSize,
                                 result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.filename, queryInfo->filename,
                                 result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.filename_regex,
                                 queryInfo->filenameRegex, result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.final_url, queryInfo->finalUrl,
                                 result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.final_url_regex,
                                 queryInfo->finalUrlRegex, result);
  EXTENSION_OP_PARAMS_GET_INT(params->query.id, queryInfo->id, result);
  EXTENSION_OP_PARAMS_GET_INT(params->query.limit, queryInfo->limit, result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.mime, queryInfo->mime, result);

  OP_PARAMS_ALLOCATE_MAMORY_VECTOR_STR(params->query.order_by,
                                       queryInfo->orderBy, result);
  OP_PARAMS_COPY_VECTOR_STRINGS(queryInfo->orderBy, result);

  EXTENSION_OP_PARAMS_GET_BOOL(params->query.paused, queryInfo->paused, result);

  OP_PARAMS_ALLOCATE_MAMORY_VECTOR_STR(params->query.order_by, queryInfo->query,
                                       result);
  OP_PARAMS_COPY_VECTOR_STRINGS(queryInfo->query, result);

  EXTENSION_OP_PARAMS_GET_STRING(params->query.start_time, queryInfo->startTime,
                                 result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.started_after,
                                 queryInfo->startedAfter, result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.started_before,
                                 queryInfo->startedBefore, result);
  EXTENSION_PARAMS_GET_ENUMINT(params->query.state, queryInfo->state, result);
  EXTENSION_OP_PARAMS_GET_DOUBLE(params->query.total_bytes,
                                 queryInfo->totalBytes, result);
  EXTENSION_OP_PARAMS_GET_DOUBLE(params->query.total_bytes_greater,
                                 queryInfo->totalBytesGreater, result);
  EXTENSION_OP_PARAMS_GET_DOUBLE(params->query.total_bytes_less,
                                 queryInfo->totalBytesLess, result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.url, queryInfo->url, result);
  EXTENSION_OP_PARAMS_GET_STRING(params->query.url_regex, queryInfo->urlRegex,
                                 result);

  return result;
}

void ExDownloadsReleaseVectorCharType(ExDownloadsVectorCharType* value) {
  if (!value) {
    return;
  }
  if (value->strs) {
    for (uint32_t i = 0; i < value->count; ++i) {
      NWEB_DOWNLOADS_SAFE_FREE(value->strs[i]);
    }
    NWEB_DOWNLOADS_SAFE_FREE(value->strs);
  }
  value->count = 0;
  NWEB_DOWNLOADS_SAFE_FREE(value);
}

void ExDownloadsQueryInfoRelease(ExDownloadsQueryInfo* queryInfo) {
  if (!queryInfo) {
    return;
  }
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->bytesReceived);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->danger);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->endTime);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->endedAfter);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->endedBefore);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->error);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->exists);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->fileSize);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->filename);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->filenameRegex);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->finalUrl);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->finalUrlRegex);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->id);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->limit);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->mime);
  ExDownloadsReleaseVectorCharType(queryInfo->orderBy);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->paused);
  ExDownloadsReleaseVectorCharType(queryInfo->query);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->startTime);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->startedAfter);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->startedBefore);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->state);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->totalBytes);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->totalBytesGreater);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->totalBytesLess);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->url);
  NWEB_DOWNLOADS_SAFE_FREE(queryInfo->urlRegex);
}

/* ui options */
bool GetDownloadUiOptionsParams(bool enabled,
                                const extensions::ExtensionId& extensionId,
                                ExDownloadsUiOptions* options) {
  LOG(INFO) << "GetDownloadUiOptionsParams param enabled: " << enabled;
  bool result = true;
  EXTENSION_PARAMS_GET_BOOL(enabled, options->enabled, result);
  EXTENSION_PARAMS_GET_STRING(extensionId, options->extensionsId, result);

  return result;
}

void ExDownloadsUiOptionsRelease(ExDownloadsUiOptions* options) {
  NWEB_DOWNLOADS_SAFE_FREE(options->enabled);
  NWEB_DOWNLOADS_SAFE_FREE(options->extensionsId);
}

/* icon options */
bool GetFileIconOptionsParams(
    const std::optional<downloads::GetFileIcon::Params>& params,
    ExDownloadsGetFileIcon* option) {
  bool result = true;
  EXTENSION_PARAMS_GET_INT(params->download_id, option->downloadId, result);
  LOG(INFO) << "GetFileIconOptionsParams params downloadId: "
            << params->download_id;
  LOG(INFO) << "GetFileIconOptionsParams option downloadId: "
            << *option->downloadId;
  if (params->options.has_value()) {
    EXTENSION_OP_PARAMS_GET_INT(params->options->size, option->size, result)
    LOG(INFO) << "GetFileIconOptionsParams params size: "
              << params->options->size.value();
    LOG(INFO) << "GetFileIconOptionsParams option size: " << *option->size;
  }
  return result;
}

void ExDownloadsIconOptionRelease(ExDownloadsGetFileIcon* option) {
  NWEB_DOWNLOADS_SAFE_FREE(option->downloadId);
  NWEB_DOWNLOADS_SAFE_FREE(option->size);
}

base::Value::Dict DownloadItemsToJSON(const ExDownloadsItem* downloadItem) {
  extensions::api::downloads::DownloadItem item;
  if (downloadItem->byExtensionId) {
    LOG(INFO) << "DownloadItemsToJSON byExtensionId: "
              << downloadItem->byExtensionId;
    item.by_extension_id = downloadItem->byExtensionId;
  }

  if (downloadItem->byExtensionName) {
    LOG(INFO) << "DownloadItemsToJSON byExtensionName: "
              << downloadItem->byExtensionName;
    item.by_extension_name = downloadItem->byExtensionName;
  }

  if (downloadItem->endTime) {
    LOG(INFO) << "DownloadItemsToJSON endTime: " << downloadItem->endTime;
    item.end_time = downloadItem->endTime;
  }

  if (downloadItem->error) {
    LOG(INFO) << "DownloadItemsToJSON error: " << *downloadItem->error;
    item.error = static_cast<extensions::api::downloads::InterruptReason>(
        *(downloadItem->error));
  }

  if (downloadItem->estimatedEndTime) {
    LOG(INFO) << "DownloadItemsToJSON estimatedEndTime: "
              << downloadItem->estimatedEndTime;
    item.estimated_end_time = downloadItem->estimatedEndTime;
  }

  LOG(INFO) << "DownloadItemsToJSON bytesReceived: "
            << *(downloadItem->bytesReceived);
  item.bytes_received = *(downloadItem->bytesReceived);
  LOG(INFO) << "DownloadItemsToJSON canResume: " << *(downloadItem->canResume);
  item.can_resume = *(downloadItem->canResume);
  LOG(INFO) << "DownloadItemsToJSON danger: " << *(downloadItem->danger);
  item.danger = static_cast<extensions::api::downloads::DangerType>(
      *(downloadItem->danger));
  LOG(INFO) << "DownloadItemsToJSON exists: " << *(downloadItem->exists);
  item.exists = *(downloadItem->exists);
  LOG(INFO) << "DownloadItemsToJSON fileSize: " << *(downloadItem->fileSize);
  item.file_size = *(downloadItem->fileSize);
  LOG(INFO) << "DownloadItemsToJSON filename: " << downloadItem->filename;
  item.filename = downloadItem->filename;
  LOG(INFO) << "DownloadItemsToJSON finalUrl: " << downloadItem->finalUrl;
  item.final_url = downloadItem->finalUrl;
  LOG(INFO) << "DownloadItemsToJSON id: " << *(downloadItem->id);
  item.id = *(downloadItem->id);
  LOG(INFO) << "DownloadItemsToJSON incognito: " << *(downloadItem->incognito);
  item.incognito = *(downloadItem->incognito);
  LOG(INFO) << "DownloadItemsToJSON mime: " << downloadItem->mime;
  item.mime = downloadItem->mime;
  LOG(INFO) << "DownloadItemsToJSON paused: " << *(downloadItem->paused);
  item.paused = *(downloadItem->paused);
  LOG(INFO) << "DownloadItemsToJSON referrer: " << downloadItem->referrer;
  item.referrer = downloadItem->referrer;
  LOG(INFO) << "DownloadItemsToJSON startTime: " << downloadItem->startTime;
  item.start_time = downloadItem->startTime;
  LOG(INFO) << "DownloadItemsToJSON state: " << *(downloadItem->state);
  item.state =
      static_cast<extensions::api::downloads::State>(*(downloadItem->state));
  LOG(INFO) << "DownloadItemsToJSON totalBytes: "
            << *(downloadItem->totalBytes);
  item.total_bytes = *(downloadItem->totalBytes);
  LOG(INFO) << "DownloadItemsToJSON url: " << downloadItem->url;
  item.url = downloadItem->url;
  return item.ToValue();
}

/* icon */
SkColorType GetBitmapColorType(int colorValue) {
  SkColorType colorType;
  switch (colorValue) {
    case 1:
      colorType = kRGBA_8888_SkColorType;
      break;
    case 2:
      colorType = kBGRA_8888_SkColorType;
      break;
    default:
      colorType = kUnknown_SkColorType;
      break;
  }
  return colorType;
}

SkAlphaType GetBitmapAlphaType(int alphaValue) {
  SkAlphaType alphaType;
  switch (alphaValue) {
    case 1:
      alphaType = kOpaque_SkAlphaType;
      break;
    case 2:
      alphaType = kPremul_SkAlphaType;
      break;
    case 3:
      alphaType = kUnpremul_SkAlphaType;
      break;
    default:
      alphaType = kUnknown_SkAlphaType;
      break;
  }
  return alphaType;
}

}  // namespace

/* downloads.erase */
DownloadsEraseFunction::DownloadsEraseFunction() {}

DownloadsEraseFunction::~DownloadsEraseFunction() {}

ExtensionFunction::ResponseAction DownloadsEraseFunction::Run() {
  std::optional<downloads::Erase::Params> params =
      downloads::Erase::Params::Create(args());
  if (!params) {
    return RespondNow(BadMessage());
  }
  ExDownloadsQueryInfo queryInfo = {0};
  if (!GetEraseDownloadQueryInfoParams(params, &queryInfo)) {
    ExDownloadsQueryInfoRelease(&queryInfo);
    return RespondNow(BadMessage());
  }

  call_downloads_erase_ = true;
  bool success =
      OHOS::NWeb::NWebExtensionDownloadCefDelegate::GetInstance().Erase(
          &queryInfo,
          base::BindRepeating(&DownloadsEraseFunction::EraseCallback,
                              weak_ptr_factory_.GetWeakPtr()));

  call_downloads_erase_ = false;
  ExDownloadsQueryInfoRelease(&queryInfo);

  if (did_respond()) {
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    return RespondLater();
  }

  return RespondNow(Error("not support downloads.erase."));
}

void DownloadsEraseFunction::EraseCallback(
    const base::WeakPtr<DownloadsEraseFunction>& function,
    const char* error,
    const uint32_t count,
    const int* eraseIds) {
  DCHECK(function);
  if (error) {
    LOG(INFO) << "DownloadsEraseFunction EraseCallback error: " << error;
    std::string errorMessage = sizeof(error) > 0 ? error : "erase error.";
    function->Respond(function->Error(errorMessage));
  } else {
    LOG(INFO) << "DownloadsEraseFunction EraseCallback has " << count
              << " eraseIds.";
    base::Value::List json_results;
    for (uint32_t i = 0; i < count; i++) {
      json_results.Append(static_cast<int>(eraseIds[i]));
    }
    function->Respond(function->WithArguments(std::move(json_results)));
  }

  if (!function->call_downloads_erase_) {
    LOG(INFO) << "DownloadsEraseFunction Release";
    function->Release();
  }
}

/* downloads.open */
DownloadsOpenFunction::DownloadsOpenFunction() {}

DownloadsOpenFunction::~DownloadsOpenFunction() {}

ExtensionFunction::ResponseAction DownloadsOpenFunction::Run() {
  std::optional<downloads::Open::Params> params =
      downloads::Open::Params::Create(args());
  if (!params) {
    return RespondNow(BadMessage());
  }

  int downloadId = params->download_id;
  LOG(INFO) << "DownloadsOpenFunction::Run downloadId: " << downloadId;

  call_downloads_open_ = true;
  bool success =
      OHOS::NWeb::NWebExtensionDownloadCefDelegate::GetInstance().Open(
          downloadId, base::BindRepeating(&DownloadsOpenFunction::OpenCallback,
                                          weak_ptr_factory_.GetWeakPtr()));

  call_downloads_open_ = false;

  if (did_respond()) {
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    return RespondLater();
  }

  return RespondNow(Error("not support downloads.open."));
}

void DownloadsOpenFunction::OpenCallback(
    const base::WeakPtr<DownloadsOpenFunction>& function,
    const char* error) {
  DCHECK(function);
  LOG(INFO) << "DownloadsOpenFunction::OpenCallback error: "
            << (error ? error : "not exist");

  if (error) {
    std::string errorMessage = error;
    function->Respond(function->Error(errorMessage));
  } else {
    function->Respond(function->NoArguments());
  }

  if (!function->call_downloads_open_) {
    LOG(INFO) << "DownloadsOpenFunction Release";
    function->Release();
  }
}

/* downloads.removeFile */
DownloadsRemoveFileFunction::DownloadsRemoveFileFunction() {}

DownloadsRemoveFileFunction::~DownloadsRemoveFileFunction() {}

ExtensionFunction::ResponseAction DownloadsRemoveFileFunction::Run() {
  std::optional<downloads::RemoveFile::Params> params =
      downloads::RemoveFile::Params::Create(args());
  if (!params) {
    return RespondNow(BadMessage());
  }

  int downloadId = params->download_id;
  LOG(INFO) << "DownloadsRemoveFileFunction::Run downloadId: " << downloadId;

  call_downloads_remove_file_ = true;
  bool success =
      OHOS::NWeb::NWebExtensionDownloadCefDelegate::GetInstance().RemoveFile(
          downloadId,
          base::BindRepeating(&DownloadsRemoveFileFunction::RemoveFileCallback,
                              weak_ptr_factory_.GetWeakPtr()));

  call_downloads_remove_file_ = false;

  if (did_respond()) {
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    return RespondLater();
  }

  return RespondNow(Error("not support downloads.removeFile."));
}

void DownloadsRemoveFileFunction::RemoveFileCallback(
    const base::WeakPtr<DownloadsRemoveFileFunction>& function,
    const char* error) {
  DCHECK(function);
  LOG(INFO) << "DownloadsRemoveFileFunction::RemoveFileCallback error: "
            << (error ? error : "not exist");

  if (error) {
    std::string errorMessage = error;
    function->Respond(function->Error(errorMessage));
  } else {
    function->Respond(function->NoArguments());
  }

  if (!function->call_downloads_remove_file_) {
    LOG(INFO) << "DownloadsRemoveFileFunction Release";
    function->Release();
  }
}

/* downloads.pause */
DownloadsPauseFunction::DownloadsPauseFunction() {}

DownloadsPauseFunction::~DownloadsPauseFunction() {}

ExtensionFunction::ResponseAction DownloadsPauseFunction::Run() {
  std::optional<downloads::Pause::Params> params =
      downloads::Pause::Params::Create(args());
  if (!params) {
    return RespondNow(BadMessage());
  }

  int downloadId = params->download_id;
  LOG(INFO) << "DownloadsPauseFunction::Run downloadId: " << downloadId;

  call_downloads_pause_ = true;
  bool success =
      OHOS::NWeb::NWebExtensionDownloadCefDelegate::GetInstance().Pause(
          downloadId,
          base::BindRepeating(&DownloadsPauseFunction::PauseCallback,
                              weak_ptr_factory_.GetWeakPtr()));

  call_downloads_pause_ = false;

  if (did_respond()) {
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    return RespondLater();
  }

  return RespondNow(Error("not support downloads.pause."));
}

void DownloadsPauseFunction::PauseCallback(
    const base::WeakPtr<DownloadsPauseFunction>& function,
    const char* error) {
  DCHECK(function);
  LOG(INFO) << "DownloadsPauseFunction::PauseCallback error: "
            << (error ? error : "not exist");

  if (error) {
    std::string errorMessage = error;
    function->Respond(function->Error(errorMessage));
  } else {
    function->Respond(function->NoArguments());
  }

  if (!function->call_downloads_pause_) {
    LOG(INFO) << "DownloadsPauseFunction Release";
    function->Release();
  }
}

/* downloads.resume */
DownloadsResumeFunction::DownloadsResumeFunction() {}

DownloadsResumeFunction::~DownloadsResumeFunction() {}

ExtensionFunction::ResponseAction DownloadsResumeFunction::Run() {
  std::optional<downloads::Resume::Params> params =
      downloads::Resume::Params::Create(args());
  if (!params) {
    return RespondNow(BadMessage());
  }

  int downloadId = params->download_id;
  LOG(INFO) << "DownloadsResumeFunction::Run downloadId: " << downloadId;

  call_downloads_resume_ = true;
  bool success =
      OHOS::NWeb::NWebExtensionDownloadCefDelegate::GetInstance().Resume(
          downloadId,
          base::BindRepeating(&DownloadsResumeFunction::ResumeCallback,
                              weak_ptr_factory_.GetWeakPtr()));

  call_downloads_resume_ = false;

  if (did_respond()) {
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    return RespondLater();
  }

  return RespondNow(Error("not support downloads.resume."));
}

void DownloadsResumeFunction::ResumeCallback(
    const base::WeakPtr<DownloadsResumeFunction>& function,
    const char* error) {
  DCHECK(function);
  LOG(INFO) << "DownloadsResumeFunction::ResumeCallback error: "
            << (error ? error : "not exist");

  if (error) {
    std::string errorMessage = error;
    function->Respond(function->Error(errorMessage));
  } else {
    function->Respond(function->NoArguments());
  }

  if (!function->call_downloads_resume_) {
    LOG(INFO) << "DownloadsResumeFunction Release";
    function->Release();
  }
}

/* downloads.cancel */
DownloadsCancelFunction::DownloadsCancelFunction() {}

DownloadsCancelFunction::~DownloadsCancelFunction() {}

ExtensionFunction::ResponseAction DownloadsCancelFunction::Run() {
  std::optional<downloads::Cancel::Params> params =
      downloads::Cancel::Params::Create(args());
  if (!params) {
    return RespondNow(BadMessage());
  }

  int downloadId = params->download_id;
  LOG(INFO) << "DownloadsCancelFunction::Run downloadId: " << downloadId;

  call_downloads_cancel_ = true;
  bool success =
      OHOS::NWeb::NWebExtensionDownloadCefDelegate::GetInstance().Cancel(
          downloadId,
          base::BindRepeating(&DownloadsCancelFunction::CancelCallback,
                              weak_ptr_factory_.GetWeakPtr()));

  call_downloads_cancel_ = false;

  if (did_respond()) {
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    return RespondLater();
  }

  return RespondNow(Error("not support downloads.cancel."));
}

void DownloadsCancelFunction::CancelCallback(
    const base::WeakPtr<DownloadsCancelFunction>& function,
    const char* error) {
  DCHECK(function);
  LOG(INFO) << "DownloadsCancelFunction::CancelCallback error: "
            << (error ? error : "not exist");

  if (error) {
    std::string errorMessage = error;
    function->Respond(function->Error(errorMessage));
  } else {
    function->Respond(function->NoArguments());
  }

  if (!function->call_downloads_cancel_) {
    LOG(INFO) << "DownloadsCancelFunction Release";
    function->Release();
  }
}

/* downloads.acceptDanger */
DownloadsAcceptDangerFunction::DownloadsAcceptDangerFunction() {}

DownloadsAcceptDangerFunction::~DownloadsAcceptDangerFunction() {}

ExtensionFunction::ResponseAction DownloadsAcceptDangerFunction::Run() {
  std::optional<downloads::AcceptDanger::Params> params =
      downloads::AcceptDanger::Params::Create(args());
  if (!params) {
    return RespondNow(BadMessage());
  }

  int downloadId = params->download_id;
  LOG(INFO) << "DownloadsAcceptDangerFunction::Run downloadId: " << downloadId;

  call_downloads_accept_danger_ = true;
  bool success =
      OHOS::NWeb::NWebExtensionDownloadCefDelegate::GetInstance().AcceptDanger(
          downloadId, base::BindRepeating(
                          &DownloadsAcceptDangerFunction::AcceptDangerCallback,
                          weak_ptr_factory_.GetWeakPtr()));

  call_downloads_accept_danger_ = false;

  if (did_respond()) {
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    return RespondLater();
  }

  return RespondNow(Error("not support downloads.acceptDanger."));
}

void DownloadsAcceptDangerFunction::AcceptDangerCallback(
    const base::WeakPtr<DownloadsAcceptDangerFunction>& function,
    const char* error) {
  DCHECK(function);
  LOG(INFO) << "DownloadsAcceptDangerFunction::AcceptDangerCallback error: "
            << (error ? error : "not exist");

  if (error) {
    std::string errorMessage = error;
    function->Respond(function->Error(errorMessage));
  } else {
    function->Respond(function->NoArguments());
  }

  if (!function->call_downloads_accept_danger_) {
    LOG(INFO) << "DownloadsAcceptDangerFunction Release";
    function->Release();
  }
}

/* downloads.setUiOptions */
DownloadsSetUiOptionsFunction::DownloadsSetUiOptionsFunction() = default;

DownloadsSetUiOptionsFunction::~DownloadsSetUiOptionsFunction() = default;

ExtensionFunction::ResponseAction DownloadsSetUiOptionsFunction::Run() {
  std::optional<downloads::SetUiOptions::Params> params =
      downloads::SetUiOptions::Params::Create(args());
  if (!params) {
    return RespondNow(BadMessage());
  }

  ExDownloadsUiOptions options = {0};
  if (!GetDownloadUiOptionsParams(params->options.enabled, extension_id(),
                                  &options)) {
    ExDownloadsUiOptionsRelease(&options);
    return RespondNow(BadMessage());
  }

  call_downloads_set_ui_options_ = true;
  bool success =
      OHOS::NWeb::NWebExtensionDownloadCefDelegate::GetInstance().SetUiOptions(
          &options, base::BindRepeating(
                        &DownloadsSetUiOptionsFunction::SetUiOptionsCallback,
                        weak_ptr_factory_.GetWeakPtr()));

  call_downloads_set_ui_options_ = false;
  ExDownloadsUiOptionsRelease(&options);

  if (did_respond()) {
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    return RespondLater();
  }

  return RespondNow(Error("not support downloads.setUiOptions."));
}

void DownloadsSetUiOptionsFunction::SetUiOptionsCallback(
    const base::WeakPtr<DownloadsSetUiOptionsFunction>& function,
    const char* error) {
  DCHECK(function);
  LOG(INFO) << "DownloadsSetUiOptionsFunction::SetUiOptionsCallback error: "
            << (error ? error : "not exist");

  if (error) {
    std::string errorMessage = error;
    function->Respond(function->Error(errorMessage));
  } else {
    function->Respond(function->NoArguments());
  }

  if (!function->call_downloads_set_ui_options_) {
    LOG(INFO) << "DownloadsSetUiOptionsFunction Release";
    function->Release();
  }
}

/* downloads.setShelfEnabled */
DownloadsSetShelfEnabledFunction::DownloadsSetShelfEnabledFunction() {}

DownloadsSetShelfEnabledFunction::~DownloadsSetShelfEnabledFunction() {}

ExtensionFunction::ResponseAction DownloadsSetShelfEnabledFunction::Run() {
  std::optional<downloads::SetShelfEnabled::Params> params =
      downloads::SetShelfEnabled::Params::Create(args());
  if (!params) {
    return RespondNow(BadMessage());
  }

  ExDownloadsUiOptions options = {0};
  if (!GetDownloadUiOptionsParams(params->enabled, extension_id(), &options)) {
    ExDownloadsUiOptionsRelease(&options);
    return RespondNow(BadMessage());
  }

  call_downloads_set_shelf_enabled_ = true;
  bool success =
      OHOS::NWeb::NWebExtensionDownloadCefDelegate::GetInstance().SetUiOptions(
          &options,
          base::BindRepeating(
              &DownloadsSetShelfEnabledFunction::SetShelfEnabledCallback,
              weak_ptr_factory_.GetWeakPtr()));

  call_downloads_set_shelf_enabled_ = false;
  ExDownloadsUiOptionsRelease(&options);

  if (did_respond()) {
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    return RespondLater();
  }

  return RespondNow(Error("not support downloads.setShelfEnabled."));
}

void DownloadsSetShelfEnabledFunction::SetShelfEnabledCallback(
    const base::WeakPtr<DownloadsSetShelfEnabledFunction>& function,
    const char* error) {
  DCHECK(function);
  LOG(INFO) << "DownloadsSetShelfEnabledFunction::SetUiOptionsCallback error: "
            << (error ? error : "not exist");

  if (error) {
    std::string errorMessage = error;
    function->Respond(function->Error(errorMessage));
  } else {
    function->Respond(function->NoArguments());
  }

  if (!function->call_downloads_set_shelf_enabled_) {
    LOG(INFO) << "DownloadsSetShelfEnabledFunction Release";
    function->Release();
  }
}

/* downloads.show */
DownloadsShowFunction::DownloadsShowFunction() {}

DownloadsShowFunction::~DownloadsShowFunction() {}

ExtensionFunction::ResponseAction DownloadsShowFunction::Run() {
  std::optional<downloads::Show::Params> params =
      downloads::Show::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int downloadId = params->download_id;
  LOG(INFO) << "DownloadsShowFunction::Run downloadId: " << downloadId;

  call_downloads_show_ = true;
  bool success =
      OHOS::NWeb::NWebExtensionDownloadCefDelegate::GetInstance().Show(
          downloadId, base::BindRepeating(&DownloadsShowFunction::ShowCallback,
                                          weak_ptr_factory_.GetWeakPtr()));

  call_downloads_show_ = false;

  if (success) {
    AddRef();
    return RespondLater();
  }
  return RespondNow(NoArguments());
}

void DownloadsShowFunction::ShowCallback(
    const base::WeakPtr<DownloadsShowFunction>& function,
    const char* error) {
  DCHECK(function);
  LOG(INFO) << "DownloadsShowFunction::ShowCallback error: "
            << (error ? error : "not exist");

  if (error) {
    std::string errorMessage = error;
    function->Respond(function->Error(errorMessage));
  } else {
    function->Respond(function->NoArguments());
  }

  if (!function->call_downloads_show_) {
    LOG(INFO) << "DownloadsShowFunction Release";
    function->Release();
  }
}

/* downloads.showDefaultFolder*/

DownloadsShowDefaultFolderFunction::DownloadsShowDefaultFolderFunction() {}

DownloadsShowDefaultFolderFunction::~DownloadsShowDefaultFolderFunction() {}

ExtensionFunction::ResponseAction DownloadsShowDefaultFolderFunction::Run() {
  call_downloads_show_default_folder_ = true;
  OHOS::NWeb::NWebExtensionDownloadCefDelegate::GetInstance()
      .ShowDefaultFolder();

  call_downloads_show_default_folder_ = false;

  return RespondNow(NoArguments());
}

/* downloads.search */
DownloadsSearchFunction::DownloadsSearchFunction() {}

DownloadsSearchFunction::~DownloadsSearchFunction() {}

ExtensionFunction::ResponseAction DownloadsSearchFunction::Run() {
  std::optional<downloads::Search::Params> params =
      downloads::Search::Params::Create(args());
  if (!params) {
    return RespondNow(BadMessage());
  }
  ExDownloadsQueryInfo queryInfo = {0};
  if (!GetSearchDownloadQueryInfoParams(params, &queryInfo)) {
    ExDownloadsQueryInfoRelease(&queryInfo);
    return RespondNow(BadMessage());
  }

  call_downloads_search_ = true;
  bool success =
      OHOS::NWeb::NWebExtensionDownloadCefDelegate::GetInstance().Search(
          &queryInfo,
          base::BindRepeating(&DownloadsSearchFunction::SearchCallback,
                              weak_ptr_factory_.GetWeakPtr()));

  call_downloads_search_ = false;
  ExDownloadsQueryInfoRelease(&queryInfo);

  if (did_respond()) {
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    return RespondLater();
  }

  return RespondNow(Error("not support downloads.search."));
}

void DownloadsSearchFunction::SearchCallback(
    const base::WeakPtr<DownloadsSearchFunction>& function,
    const char* error,
    const uint32_t count,
    const ExDownloadsItem* downloadItems) {
  DCHECK(function);
  if (error) {
    LOG(INFO) << "DownloadsSearchFunction SearchCallback error: " << error;
    std::string errorMessage = sizeof(error) > 0 ? error : "search error.";
    function->Respond(function->Error(errorMessage));
  } else {
    LOG(INFO) << "DownloadsSearchFunction SearchCallback has " << count
              << " items.";
    base::Value::List json_results;
    for (uint32_t i = 0; i < count; i++) {
      base::Value::Dict json_item = DownloadItemsToJSON(&downloadItems[i]);
      json_results.Append(std::move(json_item));
    }
    function->Respond(function->WithArguments(std::move(json_results)));
  }

  if (!function->call_downloads_search_) {
    LOG(INFO) << "DownloadsSearchFunction Release";
    function->Release();
  }
}

/* downloads.download */
int DownloadsDownloadFunction::RespondDownlodId(const std::string& guid) {
  int downloadId =
      OHOS::NWeb::NWebExtensionDownloadCefDelegate::GetInstance().GetDownloadId(
          guid);
  LOG(INFO) << "DownloadsDownloadFunction RespondDownlodId downloadId: "
            << downloadId;

  return downloadId;
}

/* downloads.getFileIcon */
DownloadsGetFileIconFunction::DownloadsGetFileIconFunction() {}

DownloadsGetFileIconFunction::~DownloadsGetFileIconFunction() {}

ExtensionFunction::ResponseAction DownloadsGetFileIconFunction::Run() {
  LOG(INFO) << "DownloadsGetFileIconFunction::Run start";
  std::optional<downloads::GetFileIcon::Params> params =
      downloads::GetFileIcon::Params::Create(args());
  if (!params) {
    return RespondNow(BadMessage());
  }
  ExDownloadsGetFileIcon iconOption = {0};
  if (!GetFileIconOptionsParams(params, &iconOption)) {
    ExDownloadsIconOptionRelease(&iconOption);
    return RespondNow(BadMessage());
  }
  LOG(INFO) << "DownloadsGetFileIconFunction run downloadId: "
            << *iconOption.downloadId;
  LOG(INFO) << "DownloadsGetFileIconFunction run size: " << *iconOption.size;

  if (iconOption.size &&
      (*(iconOption.size) != 16 && *(iconOption.size) != 32)) {
    return RespondNow(Error("Invalid `size`. Must be either `16` or `32`."));
  }

  call_downloads_getFileIcon_ = true;
  bool success =
      OHOS::NWeb::NWebExtensionDownloadCefDelegate::GetInstance().GetFileIcon(
          &iconOption, base::BindRepeating(
                           &DownloadsGetFileIconFunction::GetFileIconCallback,
                           weak_ptr_factory_.GetWeakPtr()));

  call_downloads_getFileIcon_ = false;
  ExDownloadsIconOptionRelease(&iconOption);

  if (did_respond()) {
    return AlreadyResponded();
  }

  if (success) {
    AddRef();
    return RespondLater();
  }

  return RespondNow(Error("not support downloads.getFileIcon."));
}

void DownloadsGetFileIconFunction::GetFileIconCallback(
    const base::WeakPtr<DownloadsGetFileIconFunction>& function,
    const char* error,
    const ExDownloadsIconBitmap& iconBitmap) {
  if (error) {
    LOG(INFO) << "DownloadsGetFileIconFunction GetFileIconCallback error: "
              << error;
    std::string errorMessage = sizeof(error) > 0 ? error : "getFileIcon error.";
    function->Respond(function->Error(errorMessage));
  } else {
    int width = *iconBitmap.width;
    int height = *iconBitmap.height;
    SkColorType colorType = GetBitmapColorType(*iconBitmap.colorType);
    SkAlphaType alphaType = GetBitmapAlphaType(*iconBitmap.alphaType);
    size_t bitmapSize = iconBitmap.bitmapSize;
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::Make(width, height, colorType, alphaType));

    void* pixels = bitmap.getPixels();
    if (pixels) {
      memcpy_s(pixels, bitmapSize, iconBitmap.bitmap, bitmapSize);
    }

    std::string iconUrl = webui::GetBitmapDataUrl(bitmap);
    if (iconUrl.empty()) {
      function->Respond(function->Error("Icon not found"));
    }
    function->Respond(function->WithArguments(iconUrl));
  }

  if (!function->call_downloads_getFileIcon_) {
    function->Release();
  }
}

}  // namespace extensions