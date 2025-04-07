// Copyright 2022 The Chromium Embedded Framework Authors.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/osr/touch_selection_controller_client_osr.h"

#include <cmath>
#include <set>

#include "arkweb/build/features/features.h"
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#include "base/functional/bind.h"
#include "cef/libcef/browser/osr/render_widget_host_view_osr.h"
#include "cef/libcef/browser/osr/touch_handle_drawable_osr.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_view_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/touch_selection/touch_editing_controller.h"
#if BUILDFLAG(ARKWEB_CLIPBOARD)
#include "cef/ohos_cef_ext/libcef/browser/osr/arkweb_render_widget_host_view_osr_ext.h"
#endif

#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
#include "base/base_switches.h"
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#endif

namespace {

// Delay before showing the quick menu, in milliseconds.
#if BUILDFLAG(ARKWEB_DRAG_DROP)
constexpr int kQuickMenuDelayInMs = 0;
#else
constexpr int kQuickMenuDelayInMs = 100;
#endif

#if BUILDFLAG(ARKWEB_CLIPBOARD)
constexpr cef_quick_menu_edit_state_flags_t kMenuCommands[] = {
    QM_EDITFLAG_CAN_ELLIPSIS, QM_EDITFLAG_CAN_CUT, QM_EDITFLAG_CAN_COPY,
    QM_EDITFLAG_CAN_PASTE, QM_EDITFLAG_CAN_SELECT_ALL};
#else
constexpr cef_quick_menu_edit_state_flags_t kMenuCommands[] = {
    QM_EDITFLAG_CAN_ELLIPSIS, QM_EDITFLAG_CAN_CUT, QM_EDITFLAG_CAN_COPY,
    QM_EDITFLAG_CAN_PASTE};
#endif  // BUILDFLAG(ARKWEB_CLIPBOARD)

constexpr int kInvalidCommandId = -1;
constexpr cef_event_flags_t kEmptyEventFlags =
    static_cast<cef_event_flags_t>(0);
}  // namespace

CefRunQuickMenuCallbackImpl::~CefRunQuickMenuCallbackImpl() {
#if !BUILDFLAG(ARKWEB_CLIPBOARD)
  if (!callback_.is_null()) {
    // The callback is still pending. Cancel it now.
    if (CEF_CURRENTLY_ON_UIT()) {
      RunNow(std::move(callback_), kInvalidCommandId, kEmptyEventFlags);
    } else {
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefRunQuickMenuCallbackImpl::RunNow,
                                   std::move(callback_), kInvalidCommandId,
                                   kEmptyEventFlags));
    }
  }
#endif
}

void CefRunQuickMenuCallbackImpl::Continue(int command_id,
                                           cef_event_flags_t event_flags) {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (!callback_.is_null()) {
      RunNow(std::move(callback_), command_id, event_flags);
    }
  } else {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefRunQuickMenuCallbackImpl::Continue, this,
                                 command_id, event_flags));
  }
}

void CefRunQuickMenuCallbackImpl::Cancel() {
  Continue(kInvalidCommandId, kEmptyEventFlags);
}

void CefRunQuickMenuCallbackImpl::Disconnect() {
  callback_.Reset();
}

void CefRunQuickMenuCallbackImpl::RunNow(Callback callback,
                                         int command_id,
                                         cef_event_flags_t event_flags) {
  CEF_REQUIRE_UIT();
  std::move(callback).Run(command_id, event_flags);
}

CefTouchSelectionControllerClientOSR::CefTouchSelectionControllerClientOSR(
    CefRenderWidgetHostViewOSR* rwhv)
    : rwhv_(rwhv),
      internal_client_(rwhv),
      active_client_(&internal_client_),
      active_menu_client_(this),
      quick_menu_timer_(
          FROM_HERE,
          base::Milliseconds(kQuickMenuDelayInMs),
          base::BindRepeating(
              &CefTouchSelectionControllerClientOSR::ShowQuickMenu,
              base::Unretained(this))),
      weak_ptr_factory_(this) {
  DCHECK(rwhv_);
}

CefTouchSelectionControllerClientOSR::~CefTouchSelectionControllerClientOSR() {
  for (auto& observer : observers_) {
    observer.OnManagerWillDestroy(this);
  }
}

void CefTouchSelectionControllerClientOSR::CloseQuickMenuAndHideHandles() {
  LOG(INFO) << "Close Quick Menu And Hide Hanles.";
  CloseQuickMenu();
#if BUILDFLAG(ARKWEB_MENU)
  auto controller = rwhv_->selection_controller();
  if (controller) {
    if (!controller->GetInsertHandle() ||
        !controller->GetInsertHandle()->GetEnabled()) {
      rwhv_->selection_controller()->HideAndDisallowShowingAutomatically();
    } else if (controller->GetInsertHandle()->GetEnabled()) {
      rwhv_->selection_controller()->SetTemporarilyHidden(true);
      NotifyTouchSelectionChanged(true);
    }
  }
#else
  rwhv_->selection_controller()->HideAndDisallowShowingAutomatically();
#endif  // #if BUILDFLAG(ARKWEB_MENU)
}

void CefTouchSelectionControllerClientOSR::OnWindowMoved() {
  UpdateQuickMenu();
}

void CefTouchSelectionControllerClientOSR::OnTouchDown() {
  touch_down_ = true;
  UpdateQuickMenu();
}

void CefTouchSelectionControllerClientOSR::OnTouchUp() {
  touch_down_ = false;
#if BUILDFLAG(ARKWEB_CLIPBOARD)
  if (quick_menu_running_) {
    return;
  }
#endif
  UpdateQuickMenu();
}

void CefTouchSelectionControllerClientOSR::OnScrollStarted() {
  scroll_in_progress_ = true;
  rwhv_->selection_controller()->SetTemporarilyHidden(true);
#if BUILDFLAG(ARKWEB_DRAG_DROP)
  if (handles_hidden_by_selection_ui_) {
    LOG(INFO) << "scroll starts when the handle menu is hidden";
    UpdateQuickMenuByHandlesHidden();
    return;
  }
#endif
  UpdateQuickMenu();
}

void CefTouchSelectionControllerClientOSR::OnScrollCompleted() {
  scroll_in_progress_ = false;
  active_client_->DidScroll();
  rwhv_->selection_controller()->SetTemporarilyHidden(false);
#if BUILDFLAG(ARKWEB_DRAG_DROP)
  if (handles_hidden_by_selection_ui_) {
    LOG(INFO) << "scroll completed when the handle menu is hidden";
    UpdateQuickMenuByHandlesHidden();
    return;
  }
#endif
  UpdateQuickMenu();
}

bool CefTouchSelectionControllerClientOSR::HandleContextMenu(
    const content::ContextMenuParams& params) {
#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
  bool is_browser = base::CommandLine::ForCurrentProcess() &&
                    base::CommandLine::ForCurrentProcess()->HasSwitch(
                        switches::kEnableNwebExFreeCopy);
  bool has_select = !rwhv_->GetSelectedText().empty();
  if (is_browser && !has_select && params.selection_text.empty() &&
      !params.is_editable) {
    quick_menu_requested_ = false;
    return false;
  }
#endif
  if ((params.source_type == ui::mojom::MenuSourceType::kLongPress ||
       params.source_type == ui::mojom::MenuSourceType::kLongTap) &&
      params.is_editable && params.selection_text.empty() &&
      IsQuickMenuAvailable()) {
    quick_menu_requested_ = true;
    UpdateQuickMenu();
    return true;
  }

  bool from_touch =
      params.source_type == ui::mojom::MenuSourceType::kLongPress ||
      params.source_type == ui::mojom::MenuSourceType::kLongTap ||
      params.source_type == ui::mojom::MenuSourceType::kTouch;
#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
  if (is_browser) {
    from_touch = from_touch || params.source_type ==
                                   ui::mojom::MenuSourceType::kSelectAndCopy;
  }
#endif
  if (from_touch && !params.selection_text.empty()) {
#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
    if (is_browser) {
      if (params.source_type == ui::mojom::MenuSourceType::kSelectAndCopy) {
        quick_menu_requested_ = true;
      }
      SelectionTextNotEmpty(!params.selection_text.empty());
    }
#endif
    return true;
  }

  rwhv_->selection_controller()->HideAndDisallowShowingAutomatically();
  return false;
}

void CefTouchSelectionControllerClientOSR::DidStopFlinging() {
  OnScrollCompleted();
}

void CefTouchSelectionControllerClientOSR::OnSwipeToMoveCursorBegin() {
  rwhv_->selection_controller()->OnSwipeToMoveCursorBegin();
  OnSelectionEvent(ui::INSERTION_HANDLE_DRAG_STARTED);
}

void CefTouchSelectionControllerClientOSR::OnSwipeToMoveCursorEnd() {
  rwhv_->selection_controller()->OnSwipeToMoveCursorEnd();
  OnSelectionEvent(ui::INSERTION_HANDLE_DRAG_STOPPED);
}

void CefTouchSelectionControllerClientOSR::OnClientHitTestRegionUpdated(
    ui::TouchSelectionControllerClient* client) {
  if (client != active_client_ || !rwhv_->selection_controller() ||
      rwhv_->selection_controller()->active_status() ==
          ui::TouchSelectionController::INACTIVE) {
    return;
  }

  active_client_->DidScroll();
}

void CefTouchSelectionControllerClientOSR::UpdateClientSelectionBounds(
    const gfx::SelectionBound& start,
    const gfx::SelectionBound& end) {
  UpdateClientSelectionBounds(start, end, &internal_client_, this);
}

void CefTouchSelectionControllerClientOSR::UpdateClientSelectionBounds(
    const gfx::SelectionBound& start,
    const gfx::SelectionBound& end,
    ui::TouchSelectionControllerClient* client,
    ui::TouchSelectionMenuClient* menu_client) {
  if (client != active_client_ &&
      (start.type() == gfx::SelectionBound::EMPTY || !start.visible()) &&
      (end.type() == gfx::SelectionBound::EMPTY || !end.visible()) &&
      (manager_selection_start_.type() != gfx::SelectionBound::EMPTY ||
       manager_selection_end_.type() != gfx::SelectionBound::EMPTY)) {
    return;
  }

  active_client_ = client;
  active_menu_client_ = menu_client;
  manager_selection_start_ = start;
  manager_selection_end_ = end;

  // Notify TouchSelectionController if anything should change here. Only
  // update if the client is different and not making a change to empty, or
  // is the same client.
  GetTouchSelectionController()->OnSelectionBoundsChanged(start, end);
}

void CefTouchSelectionControllerClientOSR::InvalidateClient(
    ui::TouchSelectionControllerClient* client) {
  DCHECK(client != &internal_client_);
  if (client == active_client_) {
    active_client_ = &internal_client_;
    active_menu_client_ = this;
  }
}

ui::TouchSelectionController*
CefTouchSelectionControllerClientOSR::GetTouchSelectionController() {
  return rwhv_->selection_controller();
}

void CefTouchSelectionControllerClientOSR::AddObserver(
    TouchSelectionControllerClientManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void CefTouchSelectionControllerClientOSR::RemoveObserver(
    TouchSelectionControllerClientManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool CefTouchSelectionControllerClientOSR::IsQuickMenuAvailable() const {
  DCHECK(active_menu_client_);

  const auto is_enabled = [this](cef_quick_menu_edit_state_flags_t command) {
    return active_menu_client_->IsCommandIdEnabled(command);
  };
  return std::any_of(std::cbegin(kMenuCommands), std::cend(kMenuCommands),
                     is_enabled);
}

void CefTouchSelectionControllerClientOSR::TemporarilyCloseQuickMenu() {
  if (!quick_menu_running_) {
    return;
  }

  quick_menu_running_ = false;

  auto browser = rwhv_->browser_impl();
  if (auto handler = browser->client()->GetContextMenuHandler()) {
    handler->AsCefContextMenuHandlerExt()->OnQuickMenuDismissed(
        browser.get(), browser->GetFocusedFrame(), false);
#if BUILDFLAG(ARKWEB_DRAG_DROP)
    handles_hidden_by_selection_ui_ = false;
    NotifyTouchSelectionChanged(false);
#endif
  }
#if BUILDFLAG(ARKWEB_CLIPBOARD)
  LOG(INFO) << "Close Quick Menu Now.";
  if (browser->web_contents()) {
    browser->web_contents()->SetShowingContextMenu(false);
  }
#endif  // BUILDFLAG(ARKWEB_CLIPBOARD)
}

void CefTouchSelectionControllerClientOSR::CloseQuickMenu() {
  TemporarilyCloseQuickMenu();
#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
  if (base::CommandLine::ForCurrentProcess() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNwebExFreeCopy)) {
    SelectionTextNotEmpty(false);
  }
#endif
}
void CefTouchSelectionControllerClientOSR::ShowQuickMenu() {
  auto browser = rwhv_->browser_impl();
  if (auto handler = browser->client()->GetContextMenuHandler()) {
    gfx::RectF rect =
        rwhv_->selection_controller()->GetVisibleRectBetweenBounds();

    gfx::PointF origin = rect.origin();
    gfx::PointF bottom_right = rect.bottom_right();
    auto client_bounds = gfx::RectF(rwhv_->GetViewBounds());
    origin.SetToMax(client_bounds.origin());
    bottom_right.SetToMin(client_bounds.bottom_right());
#if !BUILDFLAG(ARKWEB_CLIPBOARD)
    if (origin.x() > bottom_right.x() || origin.y() > bottom_right.y()) {
      return;
    }
#endif  // !BUILDFLAG(ARKWEB_CLIPBOARD)
    gfx::Vector2dF diagonal = bottom_right - origin;
    gfx::SizeF size(diagonal.x(), diagonal.y());

    int quickmenuflags = 0;
    for (const auto& command : kMenuCommands) {
      if (active_menu_client_->IsCommandIdEnabled(command)) {
        quickmenuflags |= command;
      }
    }
    LOG(INFO) << "Quick Menu Flags Is = " << quickmenuflags;
    CefRefPtr<CefRunQuickMenuCallbackImpl> callbackImpl(
        new CefRunQuickMenuCallbackImpl(base::BindOnce(
            &CefTouchSelectionControllerClientOSR::ExecuteCommand,
            weak_ptr_factory_.GetWeakPtr())));

    quick_menu_running_ = true;
#if BUILDFLAG(ARKWEB_DRAG_DROP)
    handles_hidden_by_selection_ui_ = false;
#endif
#if BUILDFLAG(ARKWEB_VIBRATE)
    ui::TouchSelectionController* controller = GetTouchSelectionController();
    bool isLongPressSelectionActive = false;
    if (controller) {
      isLongPressSelectionActive = controller->IsLongPressDragSelectionActive();
      LOG(INFO) << "The selection long press active is "
                << isLongPressSelectionActive << ", clipped_selection_bounds:"
                << clipped_selection_bounds_.ToString();
    }
    if (!handler->AsCefContextMenuHandlerExt()->RunQuickMenu(
            browser, browser->GetFocusedFrame(),
            {static_cast<int>(std::round(origin.x())),
             static_cast<int>(std::round(origin.y()))},
            {static_cast<int>(std::round(size.width())),
             static_cast<int>(std::round(size.height()))},
            {clipped_selection_bounds_.x(), clipped_selection_bounds_.y(),
             clipped_selection_bounds_.width(),
             clipped_selection_bounds_.height()},
            static_cast<CefContextMenuHandler::QuickMenuEditStateFlags>(
                quickmenuflags),
            callbackImpl, false, isLongPressSelectionActive)) {
      callbackImpl->Disconnect();
      if (browser) {
        if (auto client = browser->client()) {
          if (auto render = client->GetRenderHandler()) {
            render->NotifySelectAllClicked(false);
          }
        }
      }
      CloseQuickMenu();
#endif  // BUILDFLAG(ARKWEB_VIBRATE)
#if BUILDFLAG(ARKWEB_CLIPBOARD)
    } else {
      LOG(INFO) << "Show Handle Quick Menu Success";
      if (!browser) {
        return;
      }
      if (browser->web_contents()) {
        browser->web_contents()->SetShowingContextMenu(true);
      }
      browser->SetTouchInsertHandleMenuShow(false);
#endif  // BUILDFLAG(ARKWEB_CLIPBOARD)
    }
  }
}

void CefTouchSelectionControllerClientOSR::UpdateQuickMenu() {
  // Hide the quick menu if there is any. This should happen even if the menu
  // should be shown again, in order to update its location or content.
  if (quick_menu_running_) {
#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
    TemporarilyCloseQuickMenu();
#else
    CloseQuickMenu();
#endif
  } else {
    quick_menu_timer_.Stop();
  }

  // Start timer to show quick menu if necessary.
  if (ShouldShowQuickMenu()) {
    quick_menu_timer_.Reset();
  }
}

bool CefTouchSelectionControllerClientOSR::SupportsAnimation() const {
  return false;
}

bool CefTouchSelectionControllerClientOSR::InternalClient::SupportsAnimation()
    const {
  DCHECK(false);
  return false;
}

void CefTouchSelectionControllerClientOSR::SetNeedsAnimate() {
  DCHECK(false);
}

void CefTouchSelectionControllerClientOSR::InternalClient::SetNeedsAnimate() {
  DCHECK(false);
}

void CefTouchSelectionControllerClientOSR::MoveCaret(
    const gfx::PointF& position) {
  active_client_->MoveCaret(position);
}

void CefTouchSelectionControllerClientOSR::InternalClient::MoveCaret(
    const gfx::PointF& position) {
  if (auto host_delegate = rwhv_->host()->delegate()) {
    host_delegate->MoveCaret(gfx::ToRoundedPoint(position));
  }
}

void CefTouchSelectionControllerClientOSR::MoveRangeSelectionExtent(
    const gfx::PointF& extent) {
  active_client_->MoveRangeSelectionExtent(extent);
}

void CefTouchSelectionControllerClientOSR::InternalClient::
    MoveRangeSelectionExtent(const gfx::PointF& extent) {
  if (auto host_delegate = rwhv_->host()->delegate()) {
    host_delegate->MoveRangeSelectionExtent(gfx::ToRoundedPoint(extent));
  }
}

void CefTouchSelectionControllerClientOSR::SelectBetweenCoordinates(
    const gfx::PointF& base,
    const gfx::PointF& extent) {
  active_client_->SelectBetweenCoordinates(base, extent);
}

void CefTouchSelectionControllerClientOSR::InternalClient::
    SelectBetweenCoordinates(const gfx::PointF& base,
                             const gfx::PointF& extent) {
  if (auto host_delegate = rwhv_->host()->delegate()) {
    host_delegate->SelectRange(gfx::ToRoundedPoint(base),
                               gfx::ToRoundedPoint(extent));
  }
}

void CefTouchSelectionControllerClientOSR::OnSelectionEvent(
    ui::SelectionEventType event) {
  // This function (implicitly) uses active_menu_client_, so we don't go to the
  // active view for this.
  LOG(INFO) << "Selection Event Value = " << static_cast<int32_t>(event);

  switch (event) {
    case ui::SELECTION_HANDLES_SHOWN:
      quick_menu_requested_ = true;
      [[fallthrough]];
    case ui::INSERTION_HANDLE_SHOWN:
      UpdateQuickMenu();
      break;
    case ui::SELECTION_HANDLES_CLEARED:
    case ui::INSERTION_HANDLE_CLEARED:
      quick_menu_requested_ = false;
      UpdateQuickMenu();
      break;
    case ui::SELECTION_HANDLE_DRAG_STARTED:
    case ui::INSERTION_HANDLE_DRAG_STARTED:
      handle_drag_in_progress_ = true;
      UpdateQuickMenu();
      break;
    case ui::SELECTION_HANDLE_DRAG_STOPPED:
    case ui::INSERTION_HANDLE_DRAG_STOPPED:
      handle_drag_in_progress_ = false;
      UpdateQuickMenu();
      break;
    case ui::SELECTION_HANDLES_MOVED:
    case ui::INSERTION_HANDLE_MOVED:
      break;
    case ui::INSERTION_HANDLE_TAPPED:
      quick_menu_requested_ = !quick_menu_requested_;
      UpdateQuickMenu();
      break;
#if BUILDFLAG(ARKWEB_MENU)
    case ui::SELECTION_HANDLES_UPDATEMENU:
      break;
#endif
  }
}

void CefTouchSelectionControllerClientOSR::InternalClient::OnSelectionEvent(
    ui::SelectionEventType event) {
  DCHECK(false);
}

void CefTouchSelectionControllerClientOSR::OnDragUpdate(
    const ui::TouchSelectionDraggable::Type type,
    const gfx::PointF& position) {}

void CefTouchSelectionControllerClientOSR::InternalClient::OnDragUpdate(
    const ui::TouchSelectionDraggable::Type type,
    const gfx::PointF& position) {
  DCHECK(false);
}

std::unique_ptr<ui::TouchHandleDrawable>
CefTouchSelectionControllerClientOSR::CreateDrawable() {
  return std::make_unique<CefTouchHandleDrawableOSR>(rwhv_);
}

void CefTouchSelectionControllerClientOSR::DidScroll() {}

std::unique_ptr<ui::TouchHandleDrawable>
CefTouchSelectionControllerClientOSR::InternalClient::CreateDrawable() {
  DCHECK(false);
  return nullptr;
}

void CefTouchSelectionControllerClientOSR::InternalClient::DidScroll() {
  DCHECK(false);
}

bool CefTouchSelectionControllerClientOSR::IsCommandIdEnabled(
    int command_id) const {
  bool editable = rwhv_->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE;
  bool readable = rwhv_->GetTextInputType() != ui::TEXT_INPUT_TYPE_PASSWORD;
  bool has_selection = !rwhv_->GetSelectedText().empty();
#if BUILDFLAG(ARKWEB_CLIPBOARD)
  bool has_text =
      !rwhv_->AsArkWebRenderWidgetHostViewOSRExt()->GetText().empty();
#endif
#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNwebExFreeCopy)) {
    has_selection = has_selection || isSelectionTextNotEmpty_;
  }
#endif
  switch (command_id) {
    case QM_EDITFLAG_CAN_ELLIPSIS:
      return true;  // Always allowed to show the ellipsis button.
    case QM_EDITFLAG_CAN_CUT:
      return editable && readable && has_selection;
    case QM_EDITFLAG_CAN_COPY:
      return readable && has_selection;
    case QM_EDITFLAG_CAN_PASTE: {
#if BUILDFLAG(ARKWEB_CLIPBOARD)
      bool can_paste = false;
      if (!editable) {
        LOG(INFO) << "This area is not editable.";
        return can_paste;
      }
      can_paste = ui::Clipboard::GetForCurrentThread()->HasPasteData();
      return can_paste;
#else
      std::u16string result;
      ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
          ui::EndpointType::kDefault, /*notify_if_restricted=*/false);
      ui::Clipboard::GetForCurrentThread()->ReadText(
          ui::ClipboardBuffer::kCopyPaste, &data_dst, &result);
      return editable && !result.empty();
#endif  // if BUILDFLAG(ARKWEB_CLIPBOARD)
    }
#if BUILDFLAG(ARKWEB_CLIPBOARD)
    case QM_EDITFLAG_CAN_SELECT_ALL:
      if (!editable && readable) {
        return true;
      }
      if (editable && has_text) {
        return true;
      }
      return false;
#endif  // #if BUILDFLAG(ARKWEB_CLIPBOARD)
    default:
      return false;
  }
}

void CefTouchSelectionControllerClientOSR::ExecuteCommand(int command_id,
                                                          int event_flags) {
#if BUILDFLAG(ARKWEB_DRAG_DROP)
  SetSelectAllClicked(command_id);
#endif

#if !BUILDFLAG(ARKWEB_CLIPBOARD)
  if (command_id == kInvalidCommandId) {
    LOG(ERROR) << "Quick menu command id is invaild";
    return;
  }
#endif  // #if BUILDFLAG(ARKWEB_CLIPBOARD)

#if BUILDFLAG(ARKWEB_MENU)
  if (command_id != QM_EDITFLAG_CAN_ELLIPSIS &&
      command_id != QM_EDITFLAG_CAN_SELECT_ALL) {
#else
  if (command_id != QM_EDITFLAG_CAN_ELLIPSIS) {
#endif  // BUILDFLAG(ARKWEB_MENU)
    rwhv_->selection_controller()->HideAndDisallowShowingAutomatically();
  }

  content::RenderWidgetHostDelegate* host_delegate = rwhv_->host()->delegate();
  if (!host_delegate) {
    return;
  }

  auto browser = rwhv_->browser_impl();
  if (auto handler = browser->client()->GetContextMenuHandler()) {
    if (handler->OnQuickMenuCommand(
            browser.get(), browser->GetFocusedFrame(), command_id,
            static_cast<cef_event_flags_t>(event_flags))) {
      return;
    }
  }
#if BUILDFLAG(ARKWEB_CLIPBOARD)
  absl::optional<std::u16string> value;
  LOG(INFO) << "Quick menu command id = " << command_id;
#endif  // BUILDFLAG(ARKWEB_CLIPBOARD)

  switch (command_id) {
    case QM_EDITFLAG_CAN_CUT:
      host_delegate->Cut();
      break;
    case QM_EDITFLAG_CAN_COPY:
      host_delegate->Copy();
#if BUILDFLAG(ARKWEB_NAVIGATION)
      browser->web_contents()->CollapseSelection();
#endif  // BUILDFLAG(ARKWEB_NAVIGATION)
      break;
    case QM_EDITFLAG_CAN_PASTE:
      host_delegate->Paste();
      break;
#if BUILDFLAG(ARKWEB_CLIPBOARD)
    case QM_EDITFLAG_CAN_SELECT_ALL:
      host_delegate->SelectAll();
      CloseQuickMenu();
      ShowQuickMenu();
      break;
#endif  // BUILDFLAG(ARKWEB_CLIPBOARD)
    case QM_EDITFLAG_CAN_ELLIPSIS:
      CloseQuickMenu();
      RunContextMenu();
#if BUILDFLAG(ARKWEB_CLIPBOARD)
      browser->web_contents()->CollapseSelection();
#endif  // BUILDFLAG(ARKWEB_CLIPBOARD)
      break;
    default:
      // Invalid command, do nothing.
      // Also reached when callback is destroyed/cancelled.
#if BUILDFLAG(ARKWEB_CLIPBOARD)
      browser->web_contents()->CollapseSelection();
#endif  // BUILDFLAG(ARKWEB_CLIPBOARD)
      break;
  }
}

#if BUILDFLAG(ARKWEB_DRAG_DROP)
void CefTouchSelectionControllerClientOSR::SetSelectAllClicked(int command_id) {
  if (!rwhv_) {
    return;
  }
  auto browser = rwhv_->browser_impl();
  if (!browser) {
    return;
  }
  auto client = browser->client();
  if (!client) {
    return;
  }
  auto render = client->GetRenderHandler();
  if (!render) {
    return;
  }
  if (command_id == QM_EDITFLAG_CAN_SELECT_ALL) {
    render->NotifySelectAllClicked(true);
  } else {
    render->NotifySelectAllClicked(false);
  }
}
#endif  // #ifdef ARKWEB_DRAG_DROP

void CefTouchSelectionControllerClientOSR::RunContextMenu() {
  const gfx::RectF anchor_rect =
      rwhv_->selection_controller()->GetVisibleRectBetweenBounds();
  const gfx::PointF anchor_point =
      gfx::PointF(anchor_rect.CenterPoint().x(), anchor_rect.y());
  rwhv_->host()->ShowContextMenuAtPoint(
      gfx::ToRoundedPoint(anchor_point),
      ui::mojom::MenuSourceType::kTouchEditMenu);

  // Hide selection handles after getting rect-between-bounds from touch
  // selection controller; otherwise, rect would be empty and the above
  // calculations would be invalid.
  rwhv_->selection_controller()->HideAndDisallowShowingAutomatically();
}

bool CefTouchSelectionControllerClientOSR::ShouldShowQuickMenu() {
  return quick_menu_requested_ && !touch_down_ && !scroll_in_progress_ &&
         !handle_drag_in_progress_ && IsQuickMenuAvailable();
}

std::u16string CefTouchSelectionControllerClientOSR::GetSelectedText() {
  return rwhv_->GetSelectedText();
}

#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
void CefTouchSelectionControllerClientOSR::SelectionTextNotEmpty(
    bool has_selection) {
  isSelectionTextNotEmpty_ = has_selection;
}
#endif
