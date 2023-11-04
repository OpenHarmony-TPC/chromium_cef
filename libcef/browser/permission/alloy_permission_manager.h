// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ALLOY_PERMISSION_MANAGER_H_
#define CEF_LIBCEF_BROWSER_ALLOY_PERMISSION_MANAGER_H_

#include <memory>

#include "base/containers/id_map.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "libcef/browser/browser_host_base.h"
#include "content/public/browser/permission_controller.h"

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
  void RequestPermission(content::PermissionType permission,
                        content::RenderFrameHost* render_frame_host,
                        const GURL& requesting_origin,
                        bool user_gesture,
                        base::OnceCallback<void(blink::mojom::PermissionStatus)>
                            callback) override;
  void RequestPermissions(
      const std::vector<content::PermissionType>& permissions,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForFrame(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin) override;
  void ResetPermission(content::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  SubscriptionId SubscribePermissionStatusChange(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void UnsubscribePermissionStatusChange(SubscriptionId subscription_id) override;

 protected:
  void AbortPermissionRequest(int request_id);
  void AbortPermissionRequests();

 private:
  class UnhandledRequest;
  using UnhandledRequestsMap = base::IDMap<std::unique_ptr<UnhandledRequest>>;

  void RequestPermissionByType(const content::PermissionType permission_type,
                               CefRefPtr<CefBrowserHostBase> browser,
                               UnhandledRequest* pending_request_raw,
                               const int request_id);

  void AbortPermissionRequestByType(
      const content::PermissionType permission_type,
      CefRefPtr<CefBrowserHostBase> browser,
      const GURL& requesting_origin);
  // The weak pointer to this is used to clean up any information which is
  // stored in the pending request or result cache maps. However, the callback
  // should be run regardless of whether the class is still alive so the method
  // is static.
  static void OnRequestResponseCallBack(
      const base::WeakPtr<AlloyPermissionManager>& manager,
      int request_id,
      content::PermissionType permission,
      bool allowed);

  base::WeakPtrFactory<AlloyPermissionManager> weak_ptr_factory_{this};
  UnhandledRequestsMap unhandled_requests_;
};

#endif  // CEF_LIBCEF_BROWSER_ALLOY_PERMISSION_MANAGER_H_
