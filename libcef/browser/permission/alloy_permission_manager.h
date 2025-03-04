// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ALLOY_PERMISSION_MANAGER_H_
#define CEF_LIBCEF_BROWSER_ALLOY_PERMISSION_MANAGER_H_

#include <memory>

#include "base/containers/id_map.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "libcef/browser/browser_host_base.h"

#ifdef OHOS_NOTIFICATION
#include <map>
#endif // OHOS_NOTIFICATION

namespace content {
class WebContents;
}

class AlloyPermissionManager : public content::PermissionControllerDelegate {
 public:
  AlloyPermissionManager(const AlloyPermissionManager&) = delete;
  AlloyPermissionManager& operator=(const AlloyPermissionManager&) = delete;

  AlloyPermissionManager();
  ~AlloyPermissionManager() override;

  // PermissionControllerDelegate implementation.
  void RequestPermission(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void RequestPermissions(
      const std::vector<blink::PermissionType>& permissions,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  void ResetPermission(blink::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  void RequestPermissionsFromCurrentDocument(
      const std::vector<blink::PermissionType>& permissions,
      content::RenderFrameHost* render_frame_host,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  content::PermissionResult GetPermissionResultForOriginWithoutContext(
      blink::PermissionType permission,
      const url::Origin& origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host) override;
  blink::mojom::PermissionStatus GetPermissionStatusForWorker(
      blink::PermissionType permission,
      content::RenderProcessHost* render_process_host,
      const GURL& worker_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForEmbeddedRequester(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const url::Origin& requesting_origin) override;
  SubscriptionId SubscribePermissionStatusChange(
      blink::PermissionType permission,
      content::RenderProcessHost* render_process_host,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void UnsubscribePermissionStatusChange(
      SubscriptionId subscription_id) override;

#ifdef OHOS_NOTIFICATION
  void GetPermissionStatusAsync(
      blink::PermissionType permission,
      const GURL& requesting_origin,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) override;
#endif // OHOS_NOTIFICATION

 protected:
  void AbortPermissionRequest(int request_id);
  void AbortPermissionRequests();

 private:
  class UnhandledRequest;
  using UnhandledRequestsMap = base::IDMap<std::unique_ptr<UnhandledRequest>>;

#ifdef OHOS_NOTIFICATION
  class UnhandledQuery;
  using UnhandledQuerysMap = base::IDMap<std::unique_ptr<UnhandledQuery>>;
#endif // OHOS_NOTIFICATION

  void RequestPermissionByType(const blink::PermissionType permission_type,
                               CefRefPtr<CefBrowserHostBase> browser,
                               UnhandledRequest* pending_request_raw,
                               const int request_id);
  virtual GURL LastCommittedOrigin(content::RenderFrameHost* render_frame_host);
  void AbortPermissionRequestByType(const blink::PermissionType permission_type,
                                    CefRefPtr<CefBrowserHostBase> browser,
                                    const GURL& requesting_origin);
  // The weak pointer to this is used to clean up any information which is
  // stored in the pending request or result cache maps. However, the callback
  // should be run regardless of whether the class is still alive so the method
  // is static.
  static void OnRequestResponseCallBack(
      const base::WeakPtr<AlloyPermissionManager>& manager,
      int request_id,
      blink::PermissionType permission,
      bool allowed);

#ifdef OHOS_NOTIFICATION
  static void OnQueryResponseCallBack(
      const base::WeakPtr<AlloyPermissionManager>& manager,
      int query_id,
      blink::PermissionType permission,
      int32_t status);

  UnhandledQuerysMap unhandled_querys_;

  std::map<GURL, int32_t> notification_permission_;
#endif // OHOS_NOTIFICATION

  base::WeakPtrFactory<AlloyPermissionManager> weak_ptr_factory_{this};
  UnhandledRequestsMap unhandled_requests_;
};

#endif  // CEF_LIBCEF_BROWSER_ALLOY_PERMISSION_MANAGER_H_
