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

#ifndef OHOS_PRINT_MANAGER_H_
#define OHOS_PRINT_MANAGER_H_

#include <memory>

#include "components/printing/browser/print_manager.h"
#include "components/printing/common/print.mojom-forward.h"
#include "content/public/browser/web_contents_user_data.h"
#include "printing/print_settings.h"

namespace printing {

class OhosPrintManager : public printing::PrintManager,
                         public content::WebContentsUserData<OhosPrintManager> {
 public:
  OhosPrintManager(const OhosPrintManager&) = delete;
  OhosPrintManager& operator=(const OhosPrintManager&) = delete;

  ~OhosPrintManager() override;

  static void BindPrintManagerHost(
      mojo::PendingAssociatedReceiver<printing::mojom::PrintManagerHost>
          receiver,
      content::RenderFrameHost* rfh);

  // printing::PrintManager:
  void PdfWritingDone(int page_count) override;

  bool PrintNow();

  // Updates the parameters for printing.
  void UpdateParam(std::unique_ptr<printing::PrintSettings> settings,
                   int file_descriptor,
                   PdfWritingDoneCallback callback);

 private:
  friend class content::WebContentsUserData<OhosPrintManager>;

  explicit OhosPrintManager(content::WebContents* contents);

  // mojom::PrintManagerHost:
  void DidPrintDocument(printing::mojom::DidPrintDocumentParamsPtr params,
                        DidPrintDocumentCallback callback) override;
  void GetDefaultPrintSettings(
      GetDefaultPrintSettingsCallback callback) override;
  void ScriptedPrint(printing::mojom::ScriptedPrintParamsPtr params,
                     ScriptedPrintCallback callback) override;

  static void OnDidPrintDocumentWritingDone(
      const PdfWritingDoneCallback& callback,
      DidPrintDocumentCallback did_print_document_cb,
      uint32_t page_count);

  std::unique_ptr<printing::PrintSettings> CreatePdfSettings(
      const printing::PageRanges& page_ranges);
  void DidExportPdf();
  static bool ConvertPdfToImage(const std::string& pdf_filename,
                                const std::string& image_filename);

  std::unique_ptr<printing::PrintSettings> settings_;

  int fd_ = -1;
  static int imageFd_;
  uint32_t width_ = 8270;
  uint32_t height_ = 11690;
  int dpi_ = 300; // DPI (Dots Per Inch)

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace printing

#endif  // OHOS_PRINT_MANAGER_H_
