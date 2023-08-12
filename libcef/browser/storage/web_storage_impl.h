// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_WEB_STORAGE_IMPL_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_WEB_STORAGE_IMPL_H_

#include <atomic>

#include "base/threading/thread.h"
#include "include/cef_web_storage.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/thread_util.h"
#include "storage/browser/quota/quota_manager.h"

// Implementation of the CefWebStorage interface. May be created on any
// thread.
class CefWebStorageImpl : public CefWebStorage {
 public:
  CefWebStorageImpl(const CefWebStorageImpl&) = delete;
  CefWebStorageImpl& operator=(const CefWebStorageImpl&) = delete;

  CefWebStorageImpl() {}
  void Initialize(CefBrowserContext::Getter browser_context_getter,
                  CefRefPtr<CefCompletionCallback> callback);
  void DeleteAllData() override;
  void DeleteOrigin(const CefString& origin) override;
  void GetOrigins(CefRefPtr<CefGetOriginsCallback> callback) override;
  void GetOriginQuota(
      const CefString& origin,
      CefRefPtr<CefGetOriginUsageOrQuotaCallback> callback) override;
  void GetOriginUsage(
      const CefString& origin,
      CefRefPtr<CefGetOriginUsageOrQuotaCallback> callback) override;
  using GetOriginsCallback =
      base::OnceCallback<void(const std::vector<std::string>&,
                              const std::vector<int64_t>&,
                              const std::vector<int64_t>&)>;
  using QuotaUsageCallback = base::OnceCallback<void(int64_t, int64_t)>;
  void DeleteAllDataInternal();
  void DeleteOriginInternal(const GURL& origin);
  void GetOriginsInternal(CefRefPtr<CefGetOriginsCallback> callback);
  void GetOriginUsageAndQuotaInternal(
      const CefString& origin,
      bool is_quota,
      CefRefPtr<CefGetOriginUsageOrQuotaCallback> callback);
  void OnUsageAndQuotaObtained(QuotaUsageCallback ui_callback,
                               blink::mojom::QuotaStatusCode status_code,
                               int64_t usage,
                               int64_t quota);
  bool ValidContext() const;
  void StoreOrTriggerUIInitCallback(base::OnceClosure callback);
  void GetOriginsCallbackImpl(CefRefPtr<CefGetOriginsCallback> callback,
                              const std::vector<std::string>& origin,
                              const std::vector<int64_t>& usage,
                              const std::vector<int64_t>& quota);
  void CefGetOriginUsageOrQuotaCallbackImpl(
      CefRefPtr<CefGetOriginUsageOrQuotaCallback> callback,
      bool is_quota,
      int64_t usage,
      int64_t quota);

 private:
  // Only accessed on the UI thread. Will be non-null after Initialize().
  CefBrowserContext::Getter browser_context_getter_;
  bool initialized_ = false;
  std::vector<base::OnceClosure> init_callbacks_;
  base::WeakPtrFactory<CefWebStorageImpl> weak_factory_{this};
  IMPLEMENT_REFCOUNTING(CefWebStorageImpl);
};

#endif  // CEF_LIBCEF_BROWSER_NET_SERVICE_WEB_STORAGE_IMPL_H_
