// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/frame_host_impl.h"

#include "include/cef_request.h"
#include "include/cef_stream.h"
#include "include/cef_v8.h"
#include "include/test/cef_test_helpers.h"
#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/net_service/browser_urlrequest_impl.h"
#include "libcef/common/frame_util.h"
#include "libcef/common/net/url_util.h"
#include "libcef/common/process_message_impl.h"
#include "libcef/common/process_message_smr_impl.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/string_util.h"
#include "libcef/common/task_runner_impl.h"

#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "base/strings/escape.h"

#if BUILDFLAG(IS_OHOS)
#include "content/public/browser/browsing_data_remover.h"
#include "libcef/browser/image_impl.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#endif  // BUILDFLAG(IS_OHOS)

#ifdef OHOS_CLIPBOARD
#include "skia/ext/image_operations.h"
#endif

#ifdef OHOS_NETWORK_LOAD
#include "base/strings/escape.h"
#endif

namespace {

void StringVisitCallback(CefRefPtr<CefStringVisitor> visitor,
                         base::ReadOnlySharedMemoryRegion response) {
  string_util::ExecuteWithScopedCefString(
      std::move(response),
      base::BindOnce([](CefRefPtr<CefStringVisitor> visitor,
                        const CefString& str) { visitor->Visit(str); },
                     visitor));
}

void ViewTextCallback(CefRefPtr<CefFrameHostImpl> frame,
                      base::ReadOnlySharedMemoryRegion response) {
  if (auto browser = frame->GetBrowser()) {
    string_util::ExecuteWithScopedCefString(
        std::move(response),
        base::BindOnce(
            [](CefRefPtr<CefBrowser> browser, const CefString& str) {
              static_cast<CefBrowserHostBase*>(browser.get())->ViewText(str);
            },
            browser));
  }
}

using CefFrameHostImplCommand = void (CefFrameHostImpl::*)();
using WebContentsCommand = void (content::WebContents::*)();

void ExecWebContentsCommand(CefFrameHostImpl* fh,
                            CefFrameHostImplCommand fh_func,
                            WebContentsCommand wc_func,
                            const std::string& command) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(fh_func, fh));
    return;
  }
  auto rfh = fh->GetRenderFrameHost();
  if (rfh) {
    auto web_contents = content::WebContents::FromRenderFrameHost(rfh);
    if (web_contents) {
      std::invoke(wc_func, web_contents);
      return;
    }
  }
  fh->SendCommand(command);
}

#ifdef OHOS_CLIPBOARD
const int kMaxContextImageNodeSizeIfDownScale = 1024;

bool NeedsDownScale(const gfx::Size& original_image_size) {
  if (original_image_size.width() <= kMaxContextImageNodeSizeIfDownScale &&
      original_image_size.height() <= kMaxContextImageNodeSizeIfDownScale) {
    return false;
  }
  LOG(DEBUG) << "The origin image size width: " << original_image_size.width()
             << ", height: " << original_image_size.height();
  return true;
}

SkBitmap DownScale(const SkBitmap& image) {
  if (image.isNull()) {
    return SkBitmap();
  }

  gfx::Size image_size(image.width(), image.height());
  if (!NeedsDownScale(image_size)) {
    return image;
  }

  gfx::SizeF scaled_size = gfx::SizeF(image_size);

  if (scaled_size.width() > kMaxContextImageNodeSizeIfDownScale) {
    scaled_size.Scale(kMaxContextImageNodeSizeIfDownScale /
                      scaled_size.width());
  }

  if (scaled_size.height() > kMaxContextImageNodeSizeIfDownScale) {
    scaled_size.Scale(kMaxContextImageNodeSizeIfDownScale /
                      scaled_size.height());
  }

  return skia::ImageOperations::Resize(image,
                                       skia::ImageOperations::RESIZE_GOOD,
                                       static_cast<int>(scaled_size.width()),
                                       static_cast<int>(scaled_size.height()));
}
#endif

#define EXEC_WEBCONTENTS_COMMAND(name)                  \
  ExecWebContentsCommand(this, &CefFrameHostImpl::name, \
                         &content::WebContents::name, #name);

}  // namespace

CefFrameHostImpl::CefFrameHostImpl(scoped_refptr<CefBrowserInfo> browser_info,
                                   int64_t parent_frame_id)
    : is_main_frame_(false),
      frame_id_(kInvalidFrameId),
      browser_info_(browser_info),
      is_focused_(is_main_frame_),  // The main frame always starts focused.
      parent_frame_id_(parent_frame_id) {
#if DCHECK_IS_ON()
  DCHECK(browser_info_);
  if (is_main_frame_) {
    DCHECK_EQ(parent_frame_id_, kInvalidFrameId);
  } else {
    DCHECK_GT(parent_frame_id_, 0);
  }
#endif
}

CefFrameHostImpl::CefFrameHostImpl(scoped_refptr<CefBrowserInfo> browser_info,
                                   content::RenderFrameHost* render_frame_host)
    : is_main_frame_(render_frame_host->GetParent() == nullptr),
      frame_id_(frame_util::MakeFrameId(render_frame_host->GetGlobalId())),
      browser_info_(browser_info),
      is_focused_(is_main_frame_),  // The main frame always starts focused.
      url_(render_frame_host->GetLastCommittedURL().spec()),
      name_(render_frame_host->GetFrameName()),
      parent_frame_id_(
          is_main_frame_ ? kInvalidFrameId
                         : frame_util::MakeFrameId(
                               render_frame_host->GetParent()->GetGlobalId())),
      render_frame_host_(render_frame_host) {
  DCHECK(browser_info_);
}

CefFrameHostImpl::~CefFrameHostImpl() {
  // Should have been Detached if not temporary.
  DCHECK(is_temporary() || !browser_info_);
  DCHECK(!render_frame_host_);
}

bool CefFrameHostImpl::IsValid() {
  return !!GetBrowserHostBase();
}

void CefFrameHostImpl::Undo() {
  EXEC_WEBCONTENTS_COMMAND(Undo);
}

void CefFrameHostImpl::Redo() {
  EXEC_WEBCONTENTS_COMMAND(Redo);
}

void CefFrameHostImpl::Cut() {
  EXEC_WEBCONTENTS_COMMAND(Cut);
}

void CefFrameHostImpl::Copy() {
  EXEC_WEBCONTENTS_COMMAND(Copy);
}

void CefFrameHostImpl::Paste() {
  EXEC_WEBCONTENTS_COMMAND(Paste);
}

void CefFrameHostImpl::Delete() {
  EXEC_WEBCONTENTS_COMMAND(Delete);
}

void CefFrameHostImpl::SelectAll() {
  EXEC_WEBCONTENTS_COMMAND(SelectAll);
}

void CefFrameHostImpl::ViewSource() {
  SendCommandWithResponse(
      "GetSource",
      base::BindOnce(&ViewTextCallback, CefRefPtr<CefFrameHostImpl>(this)));
}

void CefFrameHostImpl::GetSource(CefRefPtr<CefStringVisitor> visitor) {
  SendCommandWithResponse("GetSource",
                          base::BindOnce(&StringVisitCallback, visitor));
}

void CefFrameHostImpl::GetText(CefRefPtr<CefStringVisitor> visitor) {
  SendCommandWithResponse("GetText",
                          base::BindOnce(&StringVisitCallback, visitor));
}

void CefFrameHostImpl::LoadRequest(CefRefPtr<CefRequest> request) {
  auto params = cef::mojom::RequestParams::New();
  static_cast<CefRequestImpl*>(request.get())->Get(params);
  LoadRequest(std::move(params));
}

void CefFrameHostImpl::LoadURL(const CefString& url) {
  LoadURLWithExtras(url, content::Referrer(), kPageTransitionExplicit,
                    std::string());
}


void CefFrameHostImpl::PostURL(const CefString& url, const std::vector<char>& post_data) {
#ifdef OHOS_POST_URL
  LoadURLWithExtras(url, content::Referrer(), kPageTransitionExplicit,
                   "Content-Type:application/x-www-form-urlencoded", "POST", post_data);
#endif
}

void CefFrameHostImpl::ExecuteJavaScript(const CefString& jsCode,
                                         const CefString& scriptUrl,
                                         int startLine) {
  SendJavaScript(jsCode, scriptUrl, startLine);
}

bool CefFrameHostImpl::IsMain() {
  return is_main_frame_;
}

bool CefFrameHostImpl::IsFocused() {
  base::AutoLock lock_scope(state_lock_);
  return is_focused_;
}

CefString CefFrameHostImpl::GetName() {
  base::AutoLock lock_scope(state_lock_);
  return name_;
}

int64 CefFrameHostImpl::GetIdentifier() {
  base::AutoLock lock_scope(state_lock_);
  return frame_id_;
}

CefRefPtr<CefFrame> CefFrameHostImpl::GetParent() {
  int64 parent_frame_id;

  {
    base::AutoLock lock_scope(state_lock_);
    if (is_main_frame_ || parent_frame_id_ == kInvalidFrameId) {
      return nullptr;
    }
    parent_frame_id = parent_frame_id_;
  }

  auto browser = GetBrowserHostBase();
  if (browser) {
    return browser->GetFrame(parent_frame_id);
  }

  return nullptr;
}

CefString CefFrameHostImpl::GetURL() {
  base::AutoLock lock_scope(state_lock_);
  return url_;
}

CefRefPtr<CefBrowser> CefFrameHostImpl::GetBrowser() {
  return GetBrowserHostBase().get();
}

CefRefPtr<CefV8Context> CefFrameHostImpl::GetV8Context() {
  DCHECK(false) << "GetV8Context cannot be called from the browser process";
  return nullptr;
}

void CefFrameHostImpl::VisitDOM(CefRefPtr<CefDOMVisitor> visitor) {
  DCHECK(false) << "VisitDOM cannot be called from the browser process";
}

CefRefPtr<CefURLRequest> CefFrameHostImpl::CreateURLRequest(
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefURLRequestClient> client) {
  if (!request || !client) {
    return nullptr;
  }

  if (!CefTaskRunnerImpl::GetCurrentTaskRunner()) {
    DCHECK(false) << "called on invalid thread";
    return nullptr;
  }

  auto browser = GetBrowserHostBase();
  if (!browser) {
    return nullptr;
  }

  auto request_context = browser->request_context();

  CefRefPtr<CefBrowserURLRequest> impl =
      new CefBrowserURLRequest(this, request, client, request_context);
  if (impl->Start()) {
    return impl.get();
  }
  return nullptr;
}

void CefFrameHostImpl::SendProcessMessage(
    CefProcessId target_process,
    CefRefPtr<CefProcessMessage> message) {
  DCHECK_EQ(PID_RENDERER, target_process);
  DCHECK(message && message->IsValid());
  if (!message || !message->IsValid()) {
    return;
  }

  if (message->GetArgumentList() != nullptr) {
    // Invalidate the message object immediately by taking the argument list.
    auto argument_list =
        static_cast<CefProcessMessageImpl*>(message.get())->TakeArgumentList();
    SendToRenderFrame(
        __FUNCTION__,
        base::BindOnce(
            [](const CefString& name, base::Value::List argument_list,
               const RenderFrameType& render_frame) {
              render_frame->SendMessage(name, std::move(argument_list));
            },
            message->GetName(), std::move(argument_list)));
  } else {
    auto region =
        static_cast<CefProcessMessageSMRImpl*>(message.get())->TakeRegion();
    SendToRenderFrame(
        __FUNCTION__,
        base::BindOnce(
            [](const CefString& name, base::ReadOnlySharedMemoryRegion region,
               const RenderFrameType& render_frame) {
              render_frame->SendSharedMemoryRegion(name, std::move(region));
            },
            message->GetName(), std::move(region)));
  }
}

void CefFrameHostImpl::SetFocused(bool focused) {
  base::AutoLock lock_scope(state_lock_);
  is_focused_ = focused;
}

void CefFrameHostImpl::RefreshAttributes() {
  CEF_REQUIRE_UIT();

  base::AutoLock lock_scope(state_lock_);
  if (!render_frame_host_) {
    return;
  }
  url_ = render_frame_host_->GetLastCommittedURL().spec();

  // Use the assigned name if it is non-empty. This represents the name property
  // on the frame DOM element. If the assigned name is empty, revert to the
  // internal unique name. This matches the logic in render_frame_util::GetName.
  name_ = render_frame_host_->GetFrameName();
  if (name_.empty()) {
    const auto node = content::FrameTreeNode::GloballyFindByID(
        render_frame_host_->GetFrameTreeNodeId());
    if (node) {
      name_ = node->unique_name();
    }
  }

  if (!is_main_frame_) {
    parent_frame_id_ =
        frame_util::MakeFrameId(render_frame_host_->GetParent()->GetGlobalId());
  }
}

void CefFrameHostImpl::NotifyMoveOrResizeStarted() {
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce([](const RenderFrameType& render_frame) {
                      render_frame->MoveOrResizeStarted();
                    }));
}

void CefFrameHostImpl::LoadRequest(cef::mojom::RequestParamsPtr params) {
  if (!url_util::FixupGURL(params->url)) {
    return;
  }

  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce(
                        [](cef::mojom::RequestParamsPtr params,
                           const RenderFrameType& render_frame) {
                          render_frame->LoadRequest(std::move(params));
                        },
                        std::move(params)));

  auto browser = GetBrowserHostBase();
  if (browser) {
    browser->OnSetFocus(FOCUS_SOURCE_NAVIGATION);
  }
}

#ifdef OHOS_NETWORK_LOAD
void CefFrameHostImpl::LoadURLWithUserGesture(const CefString& url, bool user_gesture) {
  LoadURLWithExtras(url, content::Referrer(), kPageTransitionExplicit, std::string()
#ifdef OHOS_POST_URL
,
                    std::string(),
                    std::vector<char>()
#endif
,
                    user_gesture);
}
#endif

void CefFrameHostImpl::LoadURLWithExtras(const std::string& url,
                                         const content::Referrer& referrer,
                                         ui::PageTransition transition,
                                         const std::string& extra_headers
#ifdef OHOS_POST_URL
,
                                         const std::string& method,
                                         const std::vector<char>& post_data
#endif
#ifdef OHOS_NETWORK_LOAD
,
                                         bool user_gesture
 #endif
                                        ) {
  // Only known frame ids or kMainFrameId are supported.
  const auto frame_id = GetFrameId();
  if (frame_id < CefFrameHostImpl::kMainFrameId) {
    return;
  }

  // Any necessary fixup will occur in LoadRequest.
#ifdef OHOS_NETWORK_LOAD
  GURL gurl = url_util::FixupGURL(url);
  if (!base::StartsWith(url, "file:/") && gurl.SchemeIsFile()) {
    std::string unscaped_url_str = base::UnescapeURLComponent(gurl.spec(),
      base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
    gurl = GURL(unscaped_url_str);
  }
#else
  GURL gurl = url_util::MakeGURL(url, /*fixup=*/false);
#endif
  if (frame_id == CefFrameHostImpl::kMainFrameId) {
    // Load via the browser using NavigationController.
    auto browser = GetBrowserHostBase();
    if (browser) {
      content::OpenURLParams params(
          gurl, referrer, WindowOpenDisposition::CURRENT_TAB, transition,
          /*is_renderer_initiated=*/false);
      params.extra_headers = extra_headers;
#ifdef OHOS_NETWORK_LOAD
      params.user_gesture = user_gesture;
#endif
#ifdef OHOS_POST_URL
      if (method == "POST") {
        if (post_data.size() <= 0) {
          params.post_data = new network::ResourceRequestBody();
        } else {
          params.post_data = network::ResourceRequestBody::CreateFromBytes(
            reinterpret_cast<const char *>(&post_data[0]), post_data.size());
        }
      }
#endif
      browser->LoadMainFrameURL(params);
    }
  } else {
    auto params = cef::mojom::RequestParams::New();
    params->url = gurl;
    params->referrer =
        blink::mojom::Referrer::New(referrer.url, referrer.policy);
    params->headers = extra_headers;
#ifdef OHOS_POST_URL
    if (method == "POST") {
      params->method = method;
      if (post_data.size() <= 0) {
        params->upload_data = new network::ResourceRequestBody();
      } else {
        params->upload_data = network::ResourceRequestBody::CreateFromBytes(
          reinterpret_cast<const char *>(&post_data[0]), post_data.size());
      }
    }
#endif
    LoadRequest(std::move(params));
  }
}

void CefFrameHostImpl::SendCommand(const std::string& command) {
  DCHECK(!command.empty());
  SendToRenderFrame(__FUNCTION__, base::BindOnce(
                                      [](const std::string& command,
                                         const RenderFrameType& render_frame) {
                                        render_frame->SendCommand(command);
                                      },
                                      command));
}

void CefFrameHostImpl::SendCommandWithResponse(
    const std::string& command,
    cef::mojom::RenderFrame::SendCommandWithResponseCallback
        response_callback) {
  DCHECK(!command.empty());
  SendToRenderFrame(
      __FUNCTION__,
      base::BindOnce(
          [](const std::string& command,
             cef::mojom::RenderFrame::SendCommandWithResponseCallback
                 response_callback,
             const RenderFrameType& render_frame) {
            render_frame->SendCommandWithResponse(command,
                                                  std::move(response_callback));
          },
          command, std::move(response_callback)));
}

void CefFrameHostImpl::SendJavaScript(const std::u16string& jsCode,
                                      const std::string& scriptUrl,
                                      int startLine) {
  if (jsCode.empty()) {
    return;
  }
  if (startLine <= 0) {
    // A value of 0 is v8::Message::kNoLineNumberInfo in V8. There is code in
    // V8 that will assert on that value (e.g. V8StackTraceImpl::Frame::Frame
    // if a JS exception is thrown) so make sure |startLine| > 0.
    startLine = 1;
  }

  SendToRenderFrame(
      __FUNCTION__,
      base::BindOnce(
          [](const std::u16string& jsCode, const std::string& scriptUrl,
             int startLine, const RenderFrameType& render_frame) {
            render_frame->SendJavaScript(jsCode, scriptUrl, startLine);
          },
          jsCode, scriptUrl, startLine));
}

void CefFrameHostImpl::MaybeSendDidStopLoading() {
  auto rfh = GetRenderFrameHost();
  if (!rfh) {
    return;
  }

  // We only want to notify for the highest-level LocalFrame in this frame's
  // renderer process subtree. If this frame has a parent in the same process
  // then the notification will be sent via the parent instead.
  auto rfh_parent = rfh->GetParent();
  if (rfh_parent && rfh_parent->GetProcess() == rfh->GetProcess()) {
    return;
  }

  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce([](const RenderFrameType& render_frame) {
                      render_frame->DidStopLoading();
                    }));
}

void CefFrameHostImpl::TerminateRenderProcess(bool& result) {
  LOG(INFO) << "TerminateRenderProcess in CefFrameHostImpl start, getpid:" << getpid();
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce([](const RenderFrameType& render_frame) {
                      render_frame->TerminateRenderProcess();
                    }));
  result = true;
}

#ifdef OHOS_SCROLLBAR
void CefFrameHostImpl::UpdatePixelRatio(float ratio) {
  LOG(INFO) << "UpdatePixelRatio in browser CefFrameHostImpl start, ratio:" << ratio;
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce([](float ratio, const RenderFrameType& render_frame) {
                      render_frame->UpdatePixelRatio(ratio);
                    }, ratio));
}
#endif

void CefFrameHostImpl::ExecuteJavaScriptWithUserGestureForTests(
    const CefString& javascript) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(
            &CefFrameHostImpl::ExecuteJavaScriptWithUserGestureForTests, this,
            javascript));
    return;
  }

  content::RenderFrameHost* rfh = GetRenderFrameHost();
  if (rfh) {
    rfh->ExecuteJavaScriptWithUserGestureForTests(javascript,
                                                  base::NullCallback());
  }
}

content::RenderFrameHost* CefFrameHostImpl::GetRenderFrameHost() const {
  CEF_REQUIRE_UIT();
  return render_frame_host_;
}

bool CefFrameHostImpl::Detach(DetachReason reason) {
  CEF_REQUIRE_UIT();

  if (VLOG_IS_ON(1)) {
    std::string reason_str;
    switch (reason) {
      case DetachReason::RENDER_FRAME_DELETED:
        reason_str = "RENDER_FRAME_DELETED";
        break;
      case DetachReason::NEW_MAIN_FRAME:
        reason_str = "NEW_MAIN_FRAME";
        break;
      case DetachReason::BROWSER_DESTROYED:
        reason_str = "BROWSER_DESTROYED";
        break;
    };

    VLOG(1) << GetDebugString() << " detached (reason=" << reason_str
            << ", is_connected=" << render_frame_.is_bound() << ")";
  }

  // May be called multiple times (e.g. from CefBrowserInfo SetMainFrame and
  // RemoveFrame).
  bool first_detach = false;

  // Should not be called for temporary frames.
  DCHECK(!is_temporary());

  {
    base::AutoLock lock_scope(state_lock_);
    if (browser_info_) {
      first_detach = true;
      browser_info_ = nullptr;
    }
  }

  // In case we never attached, clean up.
  while (!queued_renderer_actions_.empty()) {
    queued_renderer_actions_.pop();
  }

  if (render_frame_.is_bound()) {
    render_frame_->FrameDetached();
  }

  render_frame_.reset();
  render_frame_host_ = nullptr;

  return first_detach;
}

void CefFrameHostImpl::MaybeReAttach(
    scoped_refptr<CefBrowserInfo> browser_info,
    content::RenderFrameHost* render_frame_host) {
  CEF_REQUIRE_UIT();
  if (render_frame_.is_bound() && render_frame_host_ == render_frame_host) {
    // Nothing to do here.
    return;
  }

  // We expect that Detach() was called previously.
  CHECK(!is_temporary());
  CHECK(!render_frame_.is_bound());
  CHECK(!render_frame_host_);

  // The RFH may change but the GlobalId should remain the same.
  CHECK_EQ(frame_id_,
           frame_util::MakeFrameId(render_frame_host->GetGlobalId()));

  {
    base::AutoLock lock_scope(state_lock_);
    browser_info_ = browser_info;
  }

  render_frame_host_ = render_frame_host;
  RefreshAttributes();

  // We expect a reconnect to be triggered via FrameAttached().
}

// kMainFrameId must be -1 to align with renderer expectations.
const int64_t CefFrameHostImpl::kMainFrameId = -1;
const int64_t CefFrameHostImpl::kFocusedFrameId = -2;
const int64_t CefFrameHostImpl::kUnspecifiedFrameId = -3;
const int64_t CefFrameHostImpl::kInvalidFrameId = -4;

// This equates to (TT_EXPLICIT | TT_DIRECT_LOAD_FLAG).
const ui::PageTransition CefFrameHostImpl::kPageTransitionExplicit =
    static_cast<ui::PageTransition>(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

int64 CefFrameHostImpl::GetFrameId() const {
  base::AutoLock lock_scope(state_lock_);
  return is_main_frame_ ? kMainFrameId : frame_id_;
}

scoped_refptr<CefBrowserInfo> CefFrameHostImpl::GetBrowserInfo() const {
  base::AutoLock lock_scope(state_lock_);
  return browser_info_;
}

CefRefPtr<CefBrowserHostBase> CefFrameHostImpl::GetBrowserHostBase() const {
  if (auto browser_info = GetBrowserInfo()) {
    return browser_info->browser();
  }
  return nullptr;
}

void CefFrameHostImpl::SendToRenderFrame(const std::string& function_name,
                                         RenderFrameAction action) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefFrameHostImpl::SendToRenderFrame, this,
                                 function_name, std::move(action)));
    return;
  }

  if (is_temporary()) {
    LOG(WARNING) << function_name
                 << " sent to temporary subframe will be ignored.";
    return;
  } else if (!render_frame_host_) {
    // We've been detached.
    LOG(WARNING) << function_name << " sent to detached " << GetDebugString()
                 << " will be ignored";
    return;
  }

  if (!render_frame_.is_bound()) {
    // Queue actions until we're notified by the renderer that it's ready to
    // handle them.
    queued_renderer_actions_.push(
        std::make_pair(function_name, std::move(action)));
    return;
  }

  std::move(action).Run(render_frame_);
}

void CefFrameHostImpl::OnRenderFrameDisconnect() {
  CEF_REQUIRE_UIT();

  // Reconnect, if any, will be triggered via FrameAttached().
  render_frame_.reset();
}

void CefFrameHostImpl::SendMessage(const std::string& name,
                                   base::Value::List arguments) {
  if (auto browser = GetBrowserHostBase()) {
    if (auto client = browser->GetClient()) {
      CefRefPtr<CefProcessMessageImpl> message(
          new CefProcessMessageImpl(name, std::move(arguments),
                                    /*read_only=*/true));
      browser->GetClient()->OnProcessMessageReceived(
          browser.get(), this, PID_RENDERER, message.get());
    }
  }
}

void CefFrameHostImpl::SendSharedMemoryRegion(
    const std::string& name,
    base::ReadOnlySharedMemoryRegion region) {
  if (auto browser = GetBrowserHostBase()) {
    if (auto client = browser->GetClient()) {
      CefRefPtr<CefProcessMessage> message(
          new CefProcessMessageSMRImpl(name, std::move(region)));
      browser->GetClient()->OnProcessMessageReceived(browser.get(), this,
                                                     PID_RENDERER, message);
    }
  }
}

void CefFrameHostImpl::FrameAttached(
    mojo::PendingRemote<cef::mojom::RenderFrame> render_frame_remote,
    bool reattached) {
  CEF_REQUIRE_UIT();
  CHECK(render_frame_remote);

  auto browser_info = GetBrowserInfo();
  if (!browser_info) {
    // Already Detached.
    return;
  }

  VLOG(1) << GetDebugString() << " " << (reattached ? "re" : "") << "connected";

  render_frame_.Bind(std::move(render_frame_remote));
  render_frame_.set_disconnect_handler(
      base::BindOnce(&CefFrameHostImpl::OnRenderFrameDisconnect, this));

  // Notify the renderer process that it can start sending messages.
  render_frame_->FrameAttachedAck();

  while (!queued_renderer_actions_.empty()) {
    std::move(queued_renderer_actions_.front().second).Run(render_frame_);
    queued_renderer_actions_.pop();
  }

  browser_info->MaybeExecuteFrameNotification(base::BindOnce(
      [](CefRefPtr<CefFrameHostImpl> self, bool reattached,
         CefRefPtr<CefFrameHandler> handler) {
        if (auto browser = self->GetBrowserHostBase()) {
          handler->OnFrameAttached(browser, self, reattached);
        }
      },
      CefRefPtr<CefFrameHostImpl>(this), reattached));
}

void CefFrameHostImpl::UpdateDraggableRegions(
    absl::optional<std::vector<cef::mojom::DraggableRegionEntryPtr>> regions) {
  auto browser = GetBrowserHostBase();
  if (!browser) {
    return;
  }

  std::vector<CefDraggableRegion> draggable_regions;
  if (regions) {
    draggable_regions.reserve(regions->size());

    for (const auto& region : *regions) {
      const auto& rect = region->bounds;
      const CefRect bounds(rect.x(), rect.y(), rect.width(), rect.height());
      draggable_regions.push_back(
          CefDraggableRegion(bounds, region->draggable));
    }
  }

  // Delegate to BrowserInfo so that current state is maintained with
  // cross-origin navigation.
  browser_info_->MaybeNotifyDraggableRegionsChanged(
      browser, this, std::move(draggable_regions));
}

std::string CefFrameHostImpl::GetDebugString() const {
  return "frame " + frame_util::GetFrameDebugString(frame_id_) +
         (is_main_frame_ ? " (main)" : " (sub)");
}

#if BUILDFLAG(IS_OHOS)
void CefFrameHostImpl::OnGetImageForContextNode(
    cef::mojom::GetImageForContextNodeParamsPtr params, int command_id) {
  CefImageImpl* image_impl = new (std::nothrow) CefImageImpl();
  if (image_impl != nullptr && params->image.width() > 0 &&
      params->image.height() > 0) {
    image_impl->AddBitmap(1.0, params->image);
  } else {
    return;
  }
  CefRefPtr<CefImage> image(image_impl);
  CefRefPtr<CefContextMenuHandler> handler = nullptr;
  auto browser = GetBrowserHostBase();
  if (!browser) {
    return;
  }
  auto client = browser->GetClient();
  if (client) {
    handler = client->GetContextMenuHandler();
  }
  if (handler) {
    handler->OnGetImageForContextNode(GetBrowser(), image, command_id);
  }
}

void CefFrameHostImpl::OnGetImageForContextNodeNull(int command_id) {
  CefRefPtr<CefImage> image(new CefImageImpl());
  CefRefPtr<CefContextMenuHandler> handler = nullptr;
  auto browser = GetBrowserHostBase();
  if (!browser) {
    return;
  }
  auto client = browser->GetClient();
  if (client) {
    handler = client->GetContextMenuHandler();
  }
  if (handler) {
    handler->OnGetImageForContextNode(GetBrowser(), image, command_id);
  }
}
#ifdef OHOS_NWEB_EX
void CefFrameHostImpl::OnGetImageFromCacheEx(
    std::string url,
    uint32_t buffer_size,
    base::ReadOnlySharedMemoryRegion region) {
  auto browser = GetBrowserHostBase();
  if (!browser) {
    return;
  }

  CefRefPtr<CefContextMenuHandler> handler;
  auto client = browser->GetClient();
  if (client) {
    handler = client->GetContextMenuHandler();
  }
  if (!handler) {
    return;
  }

  if (!region.IsValid()) {
    LOG(ERROR)
        << "OnGetImageFromCache: Read-only shared memory region is invalid";
    handler->OnGetImageFromCacheEx(nullptr, 0);
    return;
  }
  base::ReadOnlySharedMemoryMapping mapping = region.MapAt(0, buffer_size);
  if (!mapping.IsValid()) {
    LOG(ERROR)
        << "OnGetImageFromCache: Read-only shared memory mapping is invalid";
    handler->OnGetImageFromCacheEx(nullptr, 0);
    return;
  }
  uint8_t* buffer = (uint8_t*)(mapping.memory());

  LOG(DEBUG) << "OnGetImageFromCacheEx invoke";
  handler->OnGetImageFromCacheEx(buffer, buffer_size);
}
#endif

void CefFrameHostImpl::OnGetImageFromCache(
    std::string url,
    int command_id,
    uint32_t buffer_size,
    base::ReadOnlySharedMemoryRegion region) {
  auto browser = GetBrowserHostBase();
  if (!browser) {
    return;
  }

  CefRefPtr<CefContextMenuHandler> handler;
  auto client = browser->GetClient();
  if (client) {
    handler = client->GetContextMenuHandler();
  }
  if (!handler) {
    return;
  }

  CefImageImpl* image_impl = new (std::nothrow) CefImageImpl();
  if (!region.IsValid()) {
    LOG(ERROR)
        << "OnGetImageFromCache: Read-only shared memory region is invalid";
    handler->OnGetImageFromCache(image_impl, command_id);
    return;
  }
  base::ReadOnlySharedMemoryMapping mapping = region.MapAt(0, buffer_size);
  if (!mapping.IsValid()) {
    LOG(ERROR)
        << "OnGetImageFromCache: Read-only shared memory mapping is invalid";
    handler->OnGetImageFromCache(image_impl, command_id);
    return;
  }
  uint8_t* buffer = (uint8_t*)(mapping.memory());

  sk_sp<SkData> sk_data = SkData::MakeWithoutCopy(buffer, buffer_size);
  if (sk_data) {
    sk_sp<SkImage> sk_image = SkImages::DeferredFromEncodedData(sk_data);
    if (sk_image) {
      SkBitmap bitmap;
      if (sk_image->asLegacyBitmap(&bitmap)) {
#ifdef OHOS_CLIPBOARD
        SkBitmap resize_image = DownScale(bitmap);
        image_impl->AddBitmap(1.0, resize_image);
#else
        image_impl->AddBitmap(1.0, bitmap);
#endif
      }
    }
  }
  handler->OnGetImageFromCache(image_impl, command_id);
}

#ifdef OHOS_NETWORK_LOAD
  std::string CefFrameHostImpl::GetRefererValue(std::string headers) {
    std::string refererValue = "";
    std::string targetKeyword = "Referer: ";
    size_t startPos = headers.find(targetKeyword);
    if (startPos == std::string::npos) {
      return refererValue;
    }
    size_t endPos = headers.find("\r\n", startPos);
    if (endPos == std::string::npos) {
      endPos = headers.size();
    }
    refererValue = headers.substr(startPos + targetKeyword.length(), endPos
                                  - startPos - targetKeyword.length());
    return refererValue;
  }
#endif

void CefFrameHostImpl::LoadHeaderUrl(const CefString& url,
                                     const CefString& additionalHttpHeaders) {
#if defined(OHOS_NETWORK_LOAD)
  std::string refererValue = GetRefererValue(additionalHttpHeaders.ToString());
  content::Referrer referer;
  if (refererValue.empty()) {
    referer = content::Referrer();
  } else {
    GURL refererUrl = url_util::MakeGURL(refererValue, /*fixup=*/false);
    referer = content::Referrer(refererUrl, network::mojom::ReferrerPolicy::kDefault);
  }
  LoadURLWithExtras(url, referer, kPageTransitionExplicit,
                    additionalHttpHeaders);
#else
  LoadURLWithExtras(url, content::Referrer(), kPageTransitionExplicit,
                    additionalHttpHeaders);
#endif
}

void CefFrameHostImpl::SendHitEvent(
    float x,
    float y,
    float width,
    float height) {
  cef::mojom::HitEventParamsPtr hit_event =
      cef::mojom::HitEventParams::New();
  hit_event->x = x;
  hit_event->y = y;
  hit_event->width = width;
  hit_event->height = height;
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce(
                        [](cef::mojom::HitEventParamsPtr hit_event,
                           const RenderFrameType& render_frame) {
                          render_frame->SendHitEvent(std::move(hit_event));
                        },
                        std::move(hit_event)));
}

void CefFrameHostImpl::SetInitialScale(float scale) {
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce(
                        [](float scale, const RenderFrameType& render_frame) {
                          render_frame->SetInitialScale(scale);
                        },
                        scale));
}

void CefFrameHostImpl::SetOptimizeParserBudgetEnabled(bool enable) {
   SendToRenderFrame(__FUNCTION__,
                    base::BindOnce(
                        [](bool enable, const RenderFrameType& render_frame) {
                          render_frame->SetOptimizeParserBudgetEnabled(enable);
                        },
                        enable));
}

#ifdef OHOS_NETWORK_CONNINFO
void CefFrameHostImpl::SetJsOnlineProperty(bool network_up) {
  SendToRenderFrame(
      __FUNCTION__,
      base::BindOnce(
          [](bool network_up, const RenderFrameType& render_frame) {
            render_frame->SetJsOnlineProperty(network_up);
          },
          network_up));
}
#endif

void CefFrameHostImpl::GetImageForContextNode(int command_id) {
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce([](int command_id, const RenderFrameType& render_frame) {
                      render_frame->GetImageForContextNode(command_id);
                    },
                    command_id));
}

void CefFrameHostImpl::PutZoomingForTextFactor(float factor) {
  CEF_REQUIRE_UIT();
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce(
                        [](float factor, const RenderFrameType& render_frame) {
                          render_frame->PutZoomingForTextFactor(factor);
                        },
                        factor));
}

void CefFrameHostImpl::GetImagesCallback(
    CefRefPtr<CefFrameHostImpl> frame,
    CefRefPtr<CefGetImagesCallback> callback,
    bool response) {
  if (auto browser = frame->GetBrowser()) {
    callback->GetImages(response);
  }
}

void CefFrameHostImpl::GetImagesWithResponse(
    cef::mojom::RenderFrame::GetImagesWithResponseCallback response_callback) {
  SendToRenderFrame(
      __FUNCTION__,
      base::BindOnce(
          [](cef::mojom::RenderFrame::GetImagesWithResponseCallback
                 response_callback,
             const RenderFrameType& render_frame) {
            render_frame->GetImagesWithResponse(std::move(response_callback));
          },
          std::move(response_callback)));
}

void CefFrameHostImpl::GetImages(CefRefPtr<CefGetImagesCallback> callback) {
  GetImagesWithResponse(base::BindOnce(
      &CefFrameHostImpl::GetImagesCallback, base::Unretained(this),
      CefRefPtr<CefFrameHostImpl>(this), callback));
}

void CefFrameHostImpl::RemoveCache(bool include_disk_files) {
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce([](const RenderFrameType& render_frame) {
                      render_frame->RemoveCache();
                    }));

  if (include_disk_files) {
    auto browser = GetBrowserHostBase();
    if (!browser) {
      LOG(ERROR) << "RemoveCache: browser is null";
      return;
    }

    auto web_contents = browser->GetWebContents();
    if (!web_contents) {
      LOG(ERROR) << "RemoveCache: web contents is null";
      return;
    }

    content::BrowsingDataRemover* remover =
        web_contents->GetBrowserContext()->GetBrowsingDataRemover();
    remover->Remove(
        base::Time(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_CACHE,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
            content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB);
  }
}
#ifdef OHOS_PAGE_UP_DOWN
void CefFrameHostImpl::ScrollPageUpDown(bool is_up,
                                        bool is_half,
                                        float view_height) {
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce(
                        [](bool is_up, bool is_half, float view_height,
                           const RenderFrameType& render_frame) {
                          render_frame->ScrollPageUpDown(is_up, is_half,
                                                         view_height);
                        },
                        is_up, is_half, view_height));
}

#ifdef OHOS_GET_SCROLL_OFFSET
void CefFrameHostImpl::GetScrollOffset(float* offset_x,
                                       float* offset_y) {
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce(
                        [](float* offset_x, float* offset_y,
                           const RenderFrameType& render_frame) {
                          render_frame->GetScrollOffset(offset_x,
                                                        offset_y);
                        },
                        offset_x, offset_y));
}
#endif
#endif  // #ifdef OHOS_PAGE_UP_DOWN

#if defined(OHOS_INPUT_EVENTS)
void CefFrameHostImpl::ScrollTo(float x, float y) {
  SendToRenderFrame(__FUNCTION__, base::BindOnce(
                                      [](float x, float y,
                                         const RenderFrameType& render_frame) {
                                        render_frame->ScrollTo(x, y);
                                      },
                                      x, y));
}

void CefFrameHostImpl::ScrollBy(float delta_x, float delta_y) {
  SendToRenderFrame(__FUNCTION__, base::BindOnce(
                                      [](float delta_x, float delta_y,
                                         const RenderFrameType& render_frame) {
                                        render_frame->ScrollBy(delta_x,
                                                               delta_y);
                                      },
                                      delta_x, delta_y));
}

void CefFrameHostImpl::SlideScroll(float vx, float vy) {
  SendToRenderFrame(__FUNCTION__, base::BindOnce(
                                      [](float vx, float vy,
                                         const RenderFrameType& render_frame) {
                                        render_frame->SlideScroll(vx, vy);
                                      },
                                      vx, vy));
}

void CefFrameHostImpl::ZoomBy(float delta, float width, float height) {
  SendToRenderFrame(__FUNCTION__, base::BindOnce(
                                      [](float delta, float width, float height,
                                         const RenderFrameType& render_frame) {
                                        render_frame->ZoomBy(delta, width,
                                                             height);
                                      },
                                      delta, width, height));
}

void CefFrameHostImpl::SetOverscrollMode(int mode) {
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce(
                        [](int mode, const RenderFrameType& render_frame) {
                          render_frame->SetOverscrollMode(mode);
                        },
                        mode));
}

void CefFrameHostImpl::SetScrollable(bool enable) {
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce(
                        [](bool enable,
                           const RenderFrameType& render_frame) {
                          render_frame->SetScrollable(enable);
                        },
                        enable));
}

void CefFrameHostImpl::GetHitData(int& type, CefString& extra_data) {
  std::string temp_extra_data;
  if (is_temporary()) {
    extra_data = temp_extra_data;
    return;
  } else if (!render_frame_host_) {
    extra_data = temp_extra_data;
    return;
  }

  if (!render_frame_.is_bound()) {
    extra_data = temp_extra_data;
    return;
  }
  render_frame_->GetHitData(&type, &temp_extra_data);
  extra_data = temp_extra_data;
}

void CefFrameHostImpl::UpdateHitTestData(int32_t type, const std::string& extra_data) {
  hit_data_.type = type;
  hit_data_.extra_data = extra_data;
}

void CefFrameHostImpl::GetLastHitData(int& type, CefString& extra_data) {
  type = hit_data_.type;
  extra_data = hit_data_.extra_data;
}

void CefFrameHostImpl::UpdateDrawRect() {
  SendToRenderFrame(__FUNCTION__, base::BindOnce(
                                      [](const RenderFrameType& render_frame) {
                                        render_frame->UpdateDrawRect();
                                      }));
}

#if defined(OHOS_GET_SCROLL_OFFSET)
void CefFrameHostImpl::GetOverScrollOffset(float* offset_x,
                                           float* offset_y) {
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce(
                        [](float* offset_x, float* offset_y,
                           const RenderFrameType& render_frame) {
                          render_frame->GetOverScrollOffset(offset_x,
                                                            offset_y);
                        },
                        offset_x, offset_y));
}
#endif

void CefFrameHostImpl::ScrollToWithAnime(float x, float y, int32_t duration) {
  SendToRenderFrame(__FUNCTION__, base::BindOnce(
                                      [](float x, float y, int32_t duration,
                                         const RenderFrameType& render_frame) {
                                        render_frame->ScrollToWithAnime(x, y, duration);
                                      },
                                      x, y, duration));
}

void CefFrameHostImpl::ScrollByWithAnime(float delta_x, float delta_y, int32_t duration) {
  SendToRenderFrame(__FUNCTION__, base::BindOnce(
                                      [](float delta_x, float delta_y, int32_t duration,
                                         const RenderFrameType& render_frame) {
                                        render_frame->ScrollByWithAnime(delta_x,
                                                               delta_y,
                                                               duration);
                                      },
                                      delta_x, delta_y, duration));
}
#endif  // defined(OHOS_INPUT_EVENTS)

#endif  // BUILDFLAG(IS_OHOS)

void CefExecuteJavaScriptWithUserGestureForTests(CefRefPtr<CefFrame> frame,
                                                 const CefString& javascript) {
  CefFrameHostImpl* impl = static_cast<CefFrameHostImpl*>(frame.get());
  if (impl) {
    impl->ExecuteJavaScriptWithUserGestureForTests(javascript);
  }
}

#if BUILDFLAG(IS_OHOS)
void CefFrameHostImpl::ShouldOverrideUrlLoading(const std::string& url,
                                                const std::string& request_method,
                                                bool user_gesture,
                                                bool is_redirect,
                                                bool is_outermost_main_frame,
                                                cef::mojom::BrowserFrame::ShouldOverrideUrlLoadingCallback callback) {
  bool override = false;
  CefRefPtr<CefBrowserHostBase> browser_host = GetBrowserHostBase();
  if (browser_host == nullptr) {
    std::move(callback).Run(override);
    return;
  }

  if (auto client = browser_host->GetClient()) {
    if (auto handler = client->GetRequestHandler()) {
      override =  handler->ShouldOverrideUrlLoading(browser_host.get(),
                                                    url,
                                                    request_method,
                                                    user_gesture,
                                                    is_redirect,
                                                    is_outermost_main_frame);
    }
  }
  std::move(callback).Run(override);
  }
#endif  // BUILDFLAG(IS_OHOS)
