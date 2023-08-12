/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "libcef/browser/printing/ohos_print_manager.h"

#include <fcntl.h>
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
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "ohos_adapter_helper.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageEncoder.h"

namespace printing {

namespace {

const std::string pdf_fileName = "/print.pdf";
const std::string image_fileName = "/print.png";

uint32_t SaveDataToFd(int fd,
                      uint32_t page_count,
                      scoped_refptr<base::RefCountedSharedMemoryMapping> data) {
  bool result = fd > base::kInvalidFd &&
                base::IsValueInRangeForNumericType<int>(data->size());
  if (result) {
    result = base::WriteFileDescriptor(fd, *data);
  }
  if (fd > 0) {
    sync();
    close(fd);
  }

  return result ? page_count : 0;
}

int MilsToDots(int val, int dpi) {
  return static_cast<int>(printing::ConvertUnitFloat(val, 1000, dpi));
}

}  // namespace

OhosPrintManager::OhosPrintManager(content::WebContents* contents)
    : PrintManager(contents),
      content::WebContentsUserData<OhosPrintManager>(*contents) {}

OhosPrintManager::~OhosPrintManager() = default;

int OhosPrintManager::imageFd_ = -1;

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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* rfh = web_contents()->GetMainFrame();
  if (!rfh || !rfh->IsRenderFrameLive()) {
    LOG(ERROR) << "rfh is nullptr.";
    return false;
  }
  GetPrintRenderFrame(rfh)->PrintRequestedPages();
  return true;
}

void OhosPrintManager::GetDefaultPrintSettings(
    GetDefaultPrintSettingsCallback callback) {
  // Please process in the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto params = printing::mojom::PrintParams::New();

  DidExportPdf();
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
  DCHECK_LE(page_count, printing::kMaxPageCount);
  if (callback)
    callback.Run(base::checked_cast<int>(page_count));
  std::move(did_print_document_cb).Run(true);
  std::string filePath =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUserDataDir);
  std::string pdf_file = filePath + pdf_fileName;
  std::string image_file = filePath + image_fileName;
  ConvertPdfToImage(pdf_file, image_file);

  std::vector<std::string> fileList;
  std::vector<uint32_t> fdList;
  fdList.push_back(imageFd_);
  std::string taskId;
  OHOS::NWeb::OhosAdapterHelper::GetInstance()
      .GetPrintManagerInstance()
      .StartPrint(fileList, fdList, taskId);
  if (imageFd_ > 0) {
    sync();
    close(imageFd_);
  }
  base::FilePath pdf_file_path(pdf_file);
  base::DeleteFile(pdf_file_path);

  base::FilePath image_file_path(image_file);
  base::DeleteFile(image_file_path);
}

std::unique_ptr<printing::PrintSettings> OhosPrintManager::CreatePdfSettings(
    const printing::PageRanges& page_ranges) {
  auto settings = std::make_unique<printing::PrintSettings>();
  // TODO:
  // The printing subsystem needs to provide an interface to set DPI page width,
  // height, margins, etc.
  int dpi = dpi_;
  gfx::Size physical_size_device_units;
  int width_in_dots = MilsToDots(width_, dpi);
  int height_in_dots = MilsToDots(height_, dpi);
  physical_size_device_units.SetSize(width_in_dots, height_in_dots);

  gfx::Rect printable_area_device_units;

  printable_area_device_units.SetRect(0, 0, width_in_dots, height_in_dots);

  if (!page_ranges.empty())
    settings->set_ranges(page_ranges);

  settings->set_dpi(dpi);
  settings->SetPrinterPrintableArea(physical_size_device_units,
                                    printable_area_device_units, true);

  printing::PageMargins margins;
  margins.left = 0;
  margins.right = 0;
  margins.top = 0;
  margins.bottom = 0;
  settings->SetCustomMargins(margins);
  settings->set_should_print_backgrounds(true);
  return settings;
}

void OhosPrintManager::DidExportPdf() {
  std::string filePath =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUserDataDir);
  filePath += pdf_fileName;
  int fd = open(filePath.c_str(), O_RDWR | O_CREAT, (mode_t)0777);
  if (fd <= 0) {
    LOG(ERROR) << "open failed, errno = " << strerror(errno);
    return;
  }
  fd_ = fd;
}

bool OhosPrintManager::ConvertPdfToImage(const std::string& pdf_filename,
                                         const std::string& image_filename) {
  // Init PDFium library.
  FPDF_InitLibrary();

  // Open PDF.
  FPDF_DOCUMENT pdf_doc = FPDF_LoadDocument(pdf_filename.c_str(), nullptr);
  if (!pdf_doc) {
    return false;
  }

  // Get PDF pageCount.
  // int pageCount = FPDF_GetPageCount(pdf_doc);

  // Get the first PDF document.
  FPDF_PAGE pdf_page = FPDF_LoadPage(pdf_doc, 0);
  if (!pdf_page) {
    FPDF_CloseDocument(pdf_doc);
    FPDF_DestroyLibrary();
    return false;
  }

  // Get page width and height.
  int page_width = FPDF_GetPageWidth(pdf_page);
  int page_height = FPDF_GetPageHeight(pdf_page);

  // Create bitmap
  FPDF_BITMAP bitmap = FPDFBitmap_Create(page_width, page_height, 0);

  FPDFBitmap_FillRect(bitmap, 0, 0, page_width, page_height, 0xFFFFFFFF);

  FPDF_RenderPageBitmap(bitmap, pdf_page, 0, 0, page_width, page_height, 0, 0);

  // Save PNG.
  int width = FPDFBitmap_GetWidth(bitmap);
  int height = FPDFBitmap_GetHeight(bitmap);
  SkBitmap skBitmap;
  skBitmap.setInfo(SkImageInfo::Make(width, height, kBGRA_8888_SkColorType,
                                     kPremul_SkAlphaType));
  skBitmap.setPixels(FPDFBitmap_GetBuffer(bitmap));

  sk_sp<SkImage> image = SkImage::MakeFromBitmap(skBitmap);
  sk_sp<SkData> skData(image->encodeToData(SkEncodedImageFormat::kPNG, 100));

  int fd = open(image_filename.c_str(), O_RDWR | O_CREAT, (mode_t)0777);
  imageFd_ = fd;
  if (fd <= 0) {
    LOG(ERROR) << "open failed, errno = " << strerror(errno);
    FPDF_ClosePage(pdf_page);
    FPDF_CloseDocument(pdf_doc);
    FPDFBitmap_Destroy(bitmap);
    FPDF_DestroyLibrary();
    return false;
  }

  ssize_t bytes_written_total = 0;
  ssize_t size = base::checked_cast<ssize_t>(skData->size());
  for (ssize_t bytes_written_partial = 0; bytes_written_total < size;
       bytes_written_total += bytes_written_partial) {
    bytes_written_partial = write(fd, skData->bytes() + bytes_written_total,
                                  size - bytes_written_total);
  }

  FPDF_ClosePage(pdf_page);
  FPDF_CloseDocument(pdf_doc);
  FPDFBitmap_Destroy(bitmap);
  FPDF_DestroyLibrary();
  return true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OhosPrintManager);

}  // namespace printing
