// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "libcef/browser/printing/ohos_print_manager.h"

#include <fcntl.h>
#include <codecvt>
#include <locale>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/common/chrome_switches.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageEncoder.h"

namespace printing {

namespace {

constexpr int PRINT_JOB_CREATE_FILE_COMPLETED_SUCCESS = 28;
constexpr int PRINT_JOB_CREATE_FILE_COMPLETED_FAILED = 29;
const std::string PROTOCOL_PATH = "://";

uint32_t SaveDataToFd(int fd,
                      uint32_t page_count,
                      scoped_refptr<base::RefCountedSharedMemoryMapping> data) {
  bool result = fd > base::kInvalidFd &&
                base::IsValueInRangeForNumericType<int>(data->size());
  if (result) {
    result = base::WriteFileDescriptor(fd, *data);
  }
  return result ? page_count : 0;
}

int MilsToDots(int val, int dpi) {
  return static_cast<int>(printing::ConvertUnitFloat(val, 1000, dpi));
}

}  // namespace

class OhosPrintManager;

class PrintDocumentAdapterImpl
    : public OHOS::NWeb::PrintDocumentAdapterAdapter {
 public:
  PrintDocumentAdapterImpl(OhosPrintManager* ohosPrintManager)
      : ohosPrintManager_(ohosPrintManager) {}
  ~PrintDocumentAdapterImpl() = default;

  void OnStartLayoutWrite(
      const std::string& jobId,
      const OHOS::NWeb::PrintAttributesAdapter& oldAttrs,
      const OHOS::NWeb::PrintAttributesAdapter& newAttrs,
      uint32_t fd,
      std::function<void(std::string, uint32_t)> writeResultCallback) override {
    LOG(INFO) << "OhosPrintManager onStartLayoutWrite.";
    PrintAttrs printAttrs;
    printAttrs.jobId = jobId;
    printAttrs.attrs = newAttrs;
    printAttrs.fd = fd;
    printAttrs.writeResultCallback = writeResultCallback;
    if (ohosPrintManager_) {
      ohosPrintManager_->SetPrintAttrs(printAttrs);
      ohosPrintManager_->PrintPage(false);
    }
  }

  void OnJobStateChanged(const std::string& jobId, uint32_t state) override {
    LOG(INFO) << "OhosPrintManager onJobStateChanged.";
    state_ = state;
    if (ohosPrintManager_ && !isCalledOnJobStateChanged) {
      isCalledOnJobStateChanged = true;
      ohosPrintManager_->RunPrintRequestedCallback(jobId);
    }
  }

 private:
  uint32_t state_ = 0;
  bool isCalledOnJobStateChanged = false;
  OhosPrintManager* ohosPrintManager_;
};

class ApplicationPrintDocumentAdapterImpl
    : public OHOS::NWeb::PrintDocumentAdapterAdapter {
 public:
  ApplicationPrintDocumentAdapterImpl(OhosPrintManager* ohosPrintManager)
      : ohosPrintManager_(ohosPrintManager) {}
  ~ApplicationPrintDocumentAdapterImpl() = default;

  void OnStartLayoutWrite(
      const std::string& jobId,
      const OHOS::NWeb::PrintAttributesAdapter& oldAttrs,
      const OHOS::NWeb::PrintAttributesAdapter& newAttrs,
      uint32_t fd,
      std::function<void(std::string, uint32_t)> writeResultCallback) override {
    LOG(INFO) << "Application OhosPrintManager onStartLayoutWrite.";
    PrintAttrs printAttrs;
    printAttrs.jobId = jobId;
    printAttrs.attrs = newAttrs;
    printAttrs.fd = fd;
    printAttrs.writeResultCallback = writeResultCallback;
    if (ohosPrintManager_) {
      if (!isCalledBeforeEvent) {
        isCalledBeforeEvent = true;
        ohosPrintManager_->DidDispatchPrintEvent(true);
      }
      ohosPrintManager_->SetPrintAttrs(printAttrs);
      ohosPrintManager_->PrintPage(true);
    }
  }

  void OnJobStateChanged(const std::string& jobId, uint32_t state) override {
    LOG(INFO) << "Application OhosPrintManager onJobStateChanged.";
    state_ = state;
    if (ohosPrintManager_ && !isCalledOnJobStateChanged) {
      isCalledOnJobStateChanged = true;
      ohosPrintManager_->DidDispatchPrintEvent(false);
    }
  }

 private:
  uint32_t state_ = 0;
  bool isCalledBeforeEvent = false;
  bool isCalledOnJobStateChanged = false;
  OhosPrintManager* ohosPrintManager_;
};

OhosPrintManager::OhosPrintManager(content::WebContents* contents)
    : PrintManager(contents),
      content::WebContentsUserData<OhosPrintManager>(*contents) {}

OhosPrintManager::~OhosPrintManager() = default;

std::unordered_map<std::string, PrintAttrs> OhosPrintManager::printAttrsMap_{};
std::string OhosPrintManager::printJobId_ = "";
// static
void OhosPrintManager::BindPrintManagerHost(
    mojo::PendingAssociatedReceiver<printing::mojom::PrintManagerHost> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    LOG(ERROR) << "web_contents is nullptr.";
    return;
  }

  auto* print_manager = OhosPrintManager::FromWebContents(web_contents);
  if (!print_manager) {
    LOG(ERROR) << "print_manager is nullptr.";
    return;
  }

  print_manager->BindReceiver(std::move(receiver), rfh);
}

void OhosPrintManager::PdfWritingDone(int page_count) {
  pdf_writing_done_callback().Run(page_count);
  fd_ = -1;
}

bool OhosPrintManager::PrintNow() {
  LOG(INFO) << "OhosPrintManager::PrintNow";
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string printJobName = GetHtmlTitle();
  std::shared_ptr<OHOS::NWeb::PrintDocumentAdapterAdapter>
      printDocumentAdapterImpl(new PrintDocumentAdapterImpl(this));
  OHOS::NWeb::PrintAttributesAdapter printAttributesAdapter;

  int32_t ret = OHOS::NWeb::OhosAdapterHelper::GetInstance()
                    .GetPrintManagerInstance()
                    .Print(printJobName, printDocumentAdapterImpl,
                           printAttributesAdapter, token_);
  LOG(INFO) << "OhosPrintManager::PrintNow ret = " << ret;
  if (ret == -1) {
    LOG(ERROR) << "print failed";
    return false;
  }
  return true;
}

void OhosPrintManager::DidDispatchPrintEvent(bool isBefore) {
  LOG(INFO) << "OhosPrintManager::DidDispatchPrintEvent isBefore = " << isBefore;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto main_task_runner = content::GetUIThreadTaskRunner({});
  if (!main_task_runner) {
    LOG(ERROR) << "main_task_runner is nullptr";
    return;
  }
  main_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&OhosPrintManager::DidDispatchPrintEventImpl, base::Unretained(this), isBefore));
}

void OhosPrintManager::DidDispatchPrintEventImpl(bool isBefore) {
  auto* rfh = web_contents()->GetMainFrame();
  if (!rfh || !rfh->IsRenderFrameLive()) {
    LOG(ERROR) << "rfh is nullptr.";
    return;
  }
  GetPrintRenderFrame(rfh)->DidDispatchPrintEvent(isBefore);
}

void OhosPrintManager::PrintPage(bool isApplication) {
  LOG(INFO) << "OhosPrintManager::PrintPage isApplication = " << isApplication;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto main_task_runner = content::GetUIThreadTaskRunner({});
  if (!main_task_runner) {
    LOG(ERROR) << "main_task_runner is nullptr";
    return;
  }
  main_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&OhosPrintManager::PrintPageImpl, base::Unretained(this), isApplication));
}

void OhosPrintManager::PrintPageImpl(bool isApplication) {
  auto* rfh = web_contents()->GetMainFrame();
  if (!rfh || !rfh->IsRenderFrameLive()) {
    LOG(ERROR) << "rfh is nullptr.";
    if (printAttrsMap_.find(printJobId_) != printAttrsMap_.end()) {
      LOG(INFO) << "writeResultCallback PRINT_JOB_CREATE_FILE_COMPLETED_FAILED";
      printAttrsMap_[printJobId_].writeResultCallback(
          printJobId_, PRINT_JOB_CREATE_FILE_COMPLETED_FAILED);
    }
    return;
  }
  if (isApplication) {
    GetPrintRenderFrame(rfh)->ApplicationPrintRequestedPages();
  } else {
    GetPrintRenderFrame(rfh)->PrintRequestedPages();
  }
}

void OhosPrintManager::DidShowPrintDialog() {
  LOG(INFO) << "OhosPrintManager::DidShowPrintDialog";
}

void OhosPrintManager::GetDefaultPrintSettings(
    GetDefaultPrintSettingsCallback callback) {
  // Please process in the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto params = printing::mojom::PrintParams::New();

  printing::PageRanges page_ranges;
  PdfWritingDoneCallback pdf_writing_done_callback =
      base::BindRepeating([](int page_count) {
        LOG(INFO) << "OhosPrintManager::pdf_writing_done_callback page_count = "
                  << page_count;
      });
  UpdateParam(CreatePdfSettings(page_ranges), fd_, pdf_writing_done_callback);
  printing::RenderParamsFromPrintSettings(*settings_, params.get());
  params->document_cookie = cookie();
  std::move(callback).Run(std::move(params));
}

void OhosPrintManager::UpdateParam(
    std::unique_ptr<printing::PrintSettings> settings,
    int file_descriptor,
    PrintManager::PdfWritingDoneCallback callback) {
  DCHECK(settings);
  DCHECK(callback);
  settings_ = std::move(settings);
  fd_ = file_descriptor;
  set_pdf_writing_done_callback(std::move(callback));
  // Set a valid dummy cookie value.
  set_cookie(1);
}

void OhosPrintManager::ScriptedPrint(
    printing::mojom::ScriptedPrintParamsPtr scripted_params,
    ScriptedPrintCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto params = printing::mojom::PrintPagesParams::New();
  params->params = printing::mojom::PrintParams::New();

  if (scripted_params->is_scripted &&
      GetCurrentTargetFrame()->IsNestedWithinFencedFrame()) {
    LOG(ERROR) << "Script Print is not allowed.";
    std::move(callback).Run(std::move(params));
    return;
  }

  printing::RenderParamsFromPrintSettings(*settings_, params->params.get());
  params->params->document_cookie = scripted_params->cookie;
  params->pages = printing::PageRange::GetPages(settings_->ranges());
  std::move(callback).Run(std::move(params));
}

void OhosPrintManager::DidPrintDocument(
    printing::mojom::DidPrintDocumentParamsPtr params,
    DidPrintDocumentCallback callback) {
  LOG(INFO) << "OhosPrintManager::DidPrintDocument";
  if (params->document_cookie != cookie()) {
    std::move(callback).Run(false);
    return;
  }

  const printing::mojom::DidPrintContentParams& content = *params->content;
  if (!content.metafile_data_region.IsValid()) {
    NOTREACHED() << "invalid memory handle";
    web_contents()->Stop();
    PdfWritingDone(0);
    std::move(callback).Run(false);
    return;
  }

  auto data = base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(
      content.metafile_data_region);
  if (!data) {
    NOTREACHED() << "couldn't map";
    web_contents()->Stop();
    PdfWritingDone(0);
    std::move(callback).Run(false);
    return;
  }

  if (number_pages() > printing::kMaxPageCount) {
    web_contents()->Stop();
    PdfWritingDone(0);
    std::move(callback).Run(false);
    return;
  }

  DCHECK(pdf_writing_done_callback());

  base::PostTaskAndReplyWithResult(
      base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})
          .get(),
      FROM_HERE, base::BindOnce(&SaveDataToFd, fd_, number_pages(), data),
      base::BindOnce(&OhosPrintManager::OnDidPrintDocumentWritingDone,
                     pdf_writing_done_callback(), std::move(callback)));
}

// static
void OhosPrintManager::OnDidPrintDocumentWritingDone(
    const PdfWritingDoneCallback& callback,
    DidPrintDocumentCallback did_print_document_cb,
    uint32_t page_count) {
  LOG(INFO) << "OhosPrintManager::OnDidPrintDocumentWritingDone";
  DCHECK_LE(page_count, printing::kMaxPageCount);
  if (callback)
    callback.Run(base::checked_cast<int>(page_count));
  std::move(did_print_document_cb).Run(true);
  if (printAttrsMap_.find(printJobId_) != printAttrsMap_.end()) {
    LOG(INFO) << "writeResultCallback PRINT_JOB_CREATE_FILE_COMPLETED_SUCCESS";
    printAttrsMap_[printJobId_].writeResultCallback(
        printJobId_, PRINT_JOB_CREATE_FILE_COMPLETED_SUCCESS);
  }
}

std::unique_ptr<printing::PrintSettings> OhosPrintManager::CreatePdfSettings(
    const printing::PageRanges& page_ranges) {
  OHOS::NWeb::PrintAttributesAdapter newAttrs;
  if (printAttrsMap_.find(printJobId_) != printAttrsMap_.end()) {
    newAttrs = printAttrsMap_[printJobId_].attrs;
  }
  auto settings = std::make_unique<printing::PrintSettings>();
  int dpi = dpi_;
  gfx::Size physical_size_device_units;
  int width_in_dots = MilsToDots(
      newAttrs.pageSize.width ? newAttrs.pageSize.width : width_, dpi);
  int height_in_dots = MilsToDots(
      newAttrs.pageSize.height ? newAttrs.pageSize.height : height_, dpi);
  physical_size_device_units.SetSize(width_in_dots, height_in_dots);

  gfx::Rect printable_area_device_units;

  printable_area_device_units.SetRect(0, 0, width_in_dots, height_in_dots);

  if (!page_ranges.empty())
    settings->set_ranges(page_ranges);

  settings->set_dpi(dpi);
  settings->SetPrinterPrintableArea(physical_size_device_units,
                                    printable_area_device_units, true);

  printing::PageMargins margins;
  margins.left = newAttrs.margin.left;
  margins.right = newAttrs.margin.right;
  margins.top = newAttrs.margin.top;
  margins.bottom = newAttrs.margin.bottom;
  settings->SetCustomMargins(margins);
  settings->set_should_print_backgrounds(true);
  settings->SetOrientation(newAttrs.isLandscape);
  return settings;
}

void OhosPrintManager::SetPrintAttrs(const PrintAttrs printAttrs) {
  printAttrsMap_[printAttrs.jobId] = printAttrs;
  fd_ = printAttrs.fd;
  printJobId_ = printAttrs.jobId;
}

void OhosPrintManager::PrintRequested(PrintRequestedCallback callback) {
  if (!PrintNow()) {
    LOG(ERROR) << "Pulling up the printing application failed.";
    std::move(callback).Run();
    return;
  }
  cancel_ = false;
  printRequestedCallback_ = std::move(callback);
}

void OhosPrintManager::CheckCancel(CheckCancelCallback callback) {
  std::move(callback).Run(cancel_);
}

void OhosPrintManager::RunPrintRequestedCallback(const std::string& jobId) {
  LOG(INFO) << "OhosPrintManager::RunPrintRequestedCallback.";
  cancel_ = true;
  std::move(printRequestedCallback_).Run();
}

std::string OhosPrintManager::GetHtmlTitle() {
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
  std::u16string u16str = u"";
  std::string printJobName = "";
  u16str = web_contents()->GetTitle();
  printJobName = convert.to_bytes(u16str);
  printJobName = RemoveProtocol(printJobName);
  std::replace(printJobName.begin(), printJobName.end(), '/', '_');
  std::replace(printJobName.begin(), printJobName.end(), '?', '_');
  LOG(INFO) << "OhosPrintManager::GetHtmlTitle printJobName is = "
            << printJobName;
  return printJobName;
}

std::string OhosPrintManager::RemoveProtocol(const std::string& url) {
  LOG(INFO) << "OhosPrintManager::RemoveProtocol";
  std::string result = url;
  std::size_t pos = result.find(PROTOCOL_PATH);
  if (pos != std::string::npos) {
    result = result.substr(pos + PROTOCOL_PATH.size());
  }
  return result;
}

void OhosPrintManager::SetToken(void* token) {
  LOG(INFO) << "OhosPrintManager::SetToken";
  token_ = token;
}

void OhosPrintManager::CreateWebPrintDocumentAdapter(const CefString& jobName, void** webPrintDocumentAdapter) {
  *webPrintDocumentAdapter = static_cast<void*>(new ApplicationPrintDocumentAdapterImpl(this));
}
WEB_CONTENTS_USER_DATA_KEY_IMPL(OhosPrintManager);

}  // namespace printing
