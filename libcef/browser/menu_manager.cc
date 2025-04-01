// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef/libcef/browser/menu_manager.h"

#include <tuple>
#include <utility>

#include "base/logging.h"
#include "cef/grit/cef_strings.h"
#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"
#include "cef/libcef/browser/context_menu_params_impl.h"
#include "cef/libcef/browser/menu_runner.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/common/app_manager.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"

#if BUILDFLAG(ARKWEB_CLIPBOARD)
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#endif  // #if BUILDFLAG(ARKWEB_CLIPBOARD)

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
#include "base/values.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "libcef/browser/extensions/tab_extensions_util.h"
#endif // #if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)

namespace {

#if BUILDFLAG(ARKWEB_CLIPBOARD)
constexpr cef_context_menu_edit_state_flags_t kMenuCommands[] = {
    CM_EDITFLAG_CAN_CUT, CM_EDITFLAG_CAN_COPY, CM_EDITFLAG_CAN_PASTE,
    CM_EDITFLAG_CAN_DELETE, CM_EDITFLAG_CAN_SELECT_ALL}; 
#endif  // #if BUILDFLAG(ARKWEB_CLIPBOARD)

CefString GetLabel(int message_id) {
  std::u16string label =
      CefAppManager::Get()->GetContentClient()->GetLocalizedString(message_id);
  DCHECK(!label.empty());
  return label;
}

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
std::string GetTypeStr(extensions::MenuItem::Type type) {
  switch (type) {
    case extensions::MenuItem::Type::NORMAL : return "normal";
    case extensions::MenuItem::Type::CHECKBOX : return "checkbox";
    case extensions::MenuItem::Type::RADIO : return "radio";
    case extensions::MenuItem::Type::SEPARATOR : return "separator";
    default : return nullptr;
  };
}
 
std::string GetContextStr(extensions::MenuItem::Context context) {
  switch (context) {
    case extensions::MenuItem::Context::ALL : return "all";
    case extensions::MenuItem::Context::PAGE : return "page";
    case extensions::MenuItem::Context::SELECTION : return "selection";
    case extensions::MenuItem::Context::LINK : return "link";
    case extensions::MenuItem::Context::EDITABLE : return "editable";
    case extensions::MenuItem::Context::IMAGE : return "image";
    case extensions::MenuItem::Context::VIDEO : return "video";
    case extensions::MenuItem::Context::AUDIO : return "audio";
    case extensions::MenuItem::Context::FRAME : return "frame";
    case extensions::MenuItem::Context::LAUNCHER : return "launcher";
    case extensions::MenuItem::Context::BROWSER_ACTION : return "browser_action";
    case extensions::MenuItem::Context::PAGE_ACTION : return "page_action";
    case extensions::MenuItem::Context::ACTION : return "action";
    default : return nullptr;
  };
}
 
std::vector<std::string> ContextListToStrVector(const extensions::MenuItem::ContextList& contextList) {
  std::vector<std::string> result;
  for (int contextInt = extensions::MenuItem::Context::ALL;
        contextInt <= extensions::MenuItem::Context::ACTION;
        contextInt <<= 1) {
    if (contextList.Contains(static_cast<extensions::MenuItem::Context>(contextInt))) {
      result.push_back(GetContextStr(static_cast<extensions::MenuItem::Context>(contextInt)));
    }
  }
  return result;
}
 
content::BrowserContext* GetBrowserContext() {
  CefRefPtr<CefRequestContext> request_context = CefRequestContext::GetGlobalContext();
  if (!request_context) {
    LOG(ERROR) << "request context is null";
    return nullptr;
  }
 
  CefRequestContextImpl* request_context_impl =
    static_cast<CefRequestContextImpl*>(request_context.get());
  CefBrowserContext* cef_browser_context = request_context_impl->GetBrowserContext();
  if (!cef_browser_context) {
    LOG(ERROR) << "cef browser context is null";
    return nullptr;
  }
  content::BrowserContext* browser_context = cef_browser_context->AsBrowserContext();
  return browser_context;
}
 
NWebContextMenusItem GetNWebContextMenusItem(extensions::MenuItem* menu_item) {
  NWebContextMenusItem item;
  item.checked = menu_item->checked();
  item.contexts = ContextListToStrVector(menu_item->contexts());
  item.documentUrlPatterns = menu_item->document_url_str_patterns();
  item.enabled = menu_item->enabled();
  item.id = menu_item->id().string_uid;
  if (menu_item->parent_id()) {
    item.parentId = menu_item->parent_id()->string_uid;
  }
  item.targetUrlPatterns = menu_item->target_url_str_patterns();
  item.title = menu_item->title();
  item.type = GetTypeStr(menu_item->type());
  item.visible = menu_item->visible();
  item.extensionId = menu_item->extension_id();
  return item;
}
 
void SetContextMenusEventProperties(base::Value::Dict& properties, ContextMenusOnClickedData& data) {
  properties.Set("menuItemId", data.menuItemId);
  properties.Set("parentMenuItemId", data.parentMenuItemId);
  properties.Set("mediaType", data.mediaType);
  properties.Set("linkUrl", data.linkUrl);
  properties.Set("srcUrl", data.srcUrl);
  properties.Set("pageUrl", data.pageUrl);
  properties.Set("frameUrl", data.frameUrl);
  if (data.selectionText.length() > 0) {
    properties.Set("selectionText", data.selectionText);
  }
  properties.Set("editable", data.editable);
  properties.Set("wasChecked", data.wasChecked);
  properties.Set("checked", data.checked);
  properties.Set("frameId", data.frameId);
}
#endif // #if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)

const int kInvalidCommandId = -1;
const cef_event_flags_t kEmptyEventFlags = static_cast<cef_event_flags_t>(0);

class CefRunContextMenuCallbackImpl : public CefRunContextMenuCallback {
 public:
  using Callback = base::OnceCallback<void(int, cef_event_flags_t)>;

  explicit CefRunContextMenuCallbackImpl(Callback callback)
      : callback_(std::move(callback)) {}

  CefRunContextMenuCallbackImpl(const CefRunContextMenuCallbackImpl&) = delete;
  CefRunContextMenuCallbackImpl& operator=(
      const CefRunContextMenuCallbackImpl&) = delete;

  ~CefRunContextMenuCallbackImpl() override {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        RunNow(std::move(callback_), kInvalidCommandId, kEmptyEventFlags);
      } else {
        CEF_POST_TASK(CEF_UIT,
                      base::BindOnce(&CefRunContextMenuCallbackImpl::RunNow,
                                     std::move(callback_), kInvalidCommandId,
                                     kEmptyEventFlags));
      }
    }
  }

  void Continue(int command_id, cef_event_flags_t event_flags) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        RunNow(std::move(callback_), command_id, event_flags);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefRunContextMenuCallbackImpl::Continue,
                                   this, command_id, event_flags));
    }
  }

  void Cancel() override { Continue(kInvalidCommandId, kEmptyEventFlags); }

  void Disconnect() { callback_.Reset(); }

 private:
  static void RunNow(Callback callback,
                     int command_id,
                     cef_event_flags_t event_flags) {
    CEF_REQUIRE_UIT();
    std::move(callback).Run(command_id, event_flags);
  }

  Callback callback_;

  IMPLEMENT_REFCOUNTING(CefRunContextMenuCallbackImpl);
};

}  // namespace

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
CefExtensionContextMenusHandler* CefMenuManager::context_menus_handler = nullptr;
// static
void CefMenuManager::OnClickedExtensionContextMenus(const std::string& extension_id,
                                                    ContextMenusOnClickedData& data,
                                                    std::optional<NWebExtensionTab>& tab) {
  content::BrowserContext* browser_context = GetBrowserContext();
  if (!browser_context) {
    LOG(ERROR) << "browser_context is null";
    return;
  }
  extensions::MenuManager* menu_manager = extensions::MenuManager::Get(browser_context);
  if (!menu_manager) {
    LOG(ERROR) << "menu_manager is null";
    return;
  }
  extensions::EventRouter* event_router = extensions::EventRouter::Get(browser_context);
  if (!event_router) {
    LOG(ERROR) << "event_router is null";
    return;
  }
  extensions::MenuItem::Id id(browser_context->IsOffTheRecord(), extensions::MenuItem::ExtensionKey(extension_id));
  id.string_uid = data.menuItemId;
  extensions::MenuItem* item = menu_manager->GetItemById(id);
  if (!item) {
    LOG(ERROR) << "item is null";
    return;
  }
  item->SetChecked(data.checked);
  base::Value::Dict properties;
  SetContextMenusEventProperties(properties, data);
  base::Value::List args;
  args.Append(std::move(properties));
 
  if (tab) {
    args.Append(extensions::GetTabValue(*tab));
  }
 
  {
    auto event = std::make_unique<extensions::Event>(
        extensions::events::CONTEXT_MENUS,
        "contextMenus",
        args.Clone(),
        browser_context);
    event->user_gesture = extensions::EventRouter::USER_GESTURE_ENABLED;
    event_router->DispatchEventToExtension(extension_id, std::move(event));
  }
  {
    auto event = std::make_unique<extensions::Event>(
        extensions::events::CONTEXT_MENUS_ON_CLICKED,
        extensions::api::context_menus::OnClicked::kEventName,
        std::move(args),
        browser_context);
    event->user_gesture = extensions::EventRouter::USER_GESTURE_ENABLED;
    event_router->DispatchEventToExtension(extension_id, std::move(event));
  }
}
 
// static
std::vector<NWebContextMenusItem> CefMenuManager::GetAllExtensionContextMenus(
  const std::vector<std::string>& extension_ids) {
  std::vector<NWebContextMenusItem> items;
  content::BrowserContext* browser_context = GetBrowserContext();
  if (!browser_context) {
    LOG(ERROR) << "browser_context is null";
    return items;
  }
  extensions::MenuManager* menu_manager = extensions::MenuManager::Get(browser_context);
  if (!menu_manager) {
    LOG(ERROR) << "menu_manager is null";
    return items;
  }
  for (const auto& id : menu_manager->ExtensionIds()) {
    if (extension_ids.size() == 0 ||
      std::find(extension_ids.begin(), extension_ids.end(), id.extension_id) != extension_ids.end()) {
      const extensions::MenuItem::OwnedList* top_items = menu_manager->MenuItems(id);
      for (const std::unique_ptr<extensions::MenuItem>& item : *top_items) {
        GetFlattenedMenuItemSubtree(items, item);
      }
    }
  }
  return items;
}
 
// static
void CefMenuManager::GetFlattenedMenuItemSubtree(std::vector<NWebContextMenusItem>& items,
                                                  const std::unique_ptr<extensions::MenuItem>& item) {
  items.push_back(GetNWebContextMenusItem(item.get()));
  for (const auto& child : item->children()) {
    GetFlattenedMenuItemSubtree(items, child);
  }
}
 
// static
void CefMenuManager::SetContextMenusHandler(CefExtensionContextMenusHandler* handler) {
  context_menus_handler = handler;
}
 
// static
void CefMenuManager::OnContextMenusCreate(const std::string& extension_id, extensions::MenuItem* menu_item) {
  if (!context_menus_handler) {
    LOG(ERROR) << "context menus handler is null";
  }
  NWebContextMenusItem item = GetNWebContextMenusItem(menu_item);
  context_menus_handler->OnContextMenusCreate(extension_id, item);
}
 
// static
void CefMenuManager::OnContextMenusUpdate(const std::string& extension_id, extensions::MenuItem* menu_item) {
  if (!context_menus_handler) {
    LOG(ERROR) << "context menus handler is null";
  }
  NWebContextMenusItem item = GetNWebContextMenusItem(menu_item);
  context_menus_handler->OnContextMenusUpdate(extension_id, item);
}
 
// static
void CefMenuManager::OnContextMenusRemove(const std::string& extension_id, const std::string& menu_item_id) {
  if (!context_menus_handler) {
    LOG(ERROR) << "context menus handler is null";
  }
  context_menus_handler->OnContextMenusRemove(extension_id, menu_item_id);
}
 
// static
void CefMenuManager::OnContextMenusRemoveAll(const std::string& extension_id) {
  if (!context_menus_handler) {
    LOG(ERROR) << "context menus handler is null";
  }
  context_menus_handler->OnContextMenusRemoveAll(extension_id);
}
#endif // #if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)

CefMenuManager::CefMenuManager(AlloyBrowserHostImpl* browser,
                               std::unique_ptr<CefMenuRunner> runner)
    : content::WebContentsObserver(browser->web_contents()),
      browser_(browser),
      runner_(std::move(runner)),

      weak_ptr_factory_(this) {
  DCHECK(web_contents());
  model_ = new CefMenuModelImpl(this, nullptr, false);
}

CefMenuManager::~CefMenuManager() {
  // The model may outlive the delegate if the context menu is visible when the
  // application is closed.
  model_->set_delegate(nullptr);
}

void CefMenuManager::Destroy() {
  CancelContextMenu();
  if (runner_) {
    runner_.reset(nullptr);
  }
}

bool CefMenuManager::IsShowingContextMenu() {
  if (!web_contents()) {
    return false;
  }
  return web_contents()->IsShowingContextMenu();
}

#if BUILDFLAG(ARKWEB_CLIPBOARD)
bool CefMenuManager::IsCommandIdEnabled(
    int command_id,
    content::ContextMenuParams& params) const {
  bool editable = params.is_editable;
  switch (command_id) {
    case CM_EDITFLAG_CAN_CUT:
      return !!(params.edit_flags & CM_EDITFLAG_CAN_CUT);
    case CM_EDITFLAG_CAN_DELETE:
      return !!(params.edit_flags & CM_EDITFLAG_CAN_DELETE);
    case CM_EDITFLAG_CAN_COPY:
      return !!(params.edit_flags & CM_EDITFLAG_CAN_COPY);
    case CM_EDITFLAG_CAN_PASTE: {
      bool can_paste = false;
      if (!editable) {
        LOG(INFO) << "This area is not editable.";
        return can_paste;
      }
      can_paste = ui::Clipboard::GetForCurrentThread()->HasPasteData();
      return can_paste;
    }
    case CM_EDITFLAG_CAN_SELECT_ALL:
      return !!(params.edit_flags & CM_EDITFLAG_CAN_SELECT_ALL);
    default:
      return false;
  }
}

void CefMenuManager::UpdateMenuEditStateFlags(
    content::ContextMenuParams& params) {
  int menu_flags = 0;
  for (const auto& command : kMenuCommands) {
    if (IsCommandIdEnabled(command, params)) {
      menu_flags |= command;
    }
  }

  params.edit_flags = menu_flags;
}
#endif  // #if BUILDFLAG(ARKWEB_CLIPBOARD)

bool CefMenuManager::CreateContextMenu(
    const content::ContextMenuParams& params) {
  // The renderer may send the "show context menu" message multiple times, one
  // for each right click mouse event it receives. Normally, this doesn't happen
  // because mouse events are not forwarded once the context menu is showing.
  // However, there's a race - the context menu may not yet be showing when
  // the second mouse event arrives. In this case, |HandleContextMenu()| will
  // get called multiple times - if so, don't create another context menu.
  // TODO(asvitkine): Fix the renderer so that it doesn't do this.
  if (IsShowingContextMenu()) {
    return true;
  }

  params_ = params;
  model_->Clear();

#if BUILDFLAG(ARKWEB_CLIPBOARD)
  UpdateMenuEditStateFlags(params_);
#endif  // #if BUILDFLAG(ARKWEB_CLIPBOARD)

  // Create the default menu model.
  CreateDefaultModel();

  bool custom_menu = false;
  DCHECK(!custom_menu_callback_);

  // Give the client a chance to modify the model.
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefContextMenuHandler> handler = client->GetContextMenuHandler();
    if (handler.get()) {
      CefRefPtr<CefContextMenuParamsImpl> paramsPtr(
          new CefContextMenuParamsImpl(&params_));
      CefRefPtr<CefFrame> frame = browser_->GetFocusedFrame();

      handler->OnBeforeContextMenu(browser_.get(), frame, paramsPtr.get(),
                                   model_.get());

      MenuWillShow(model_);

      if (model_->GetCount() > 0) {
        CefRefPtr<CefRunContextMenuCallbackImpl> callbackImpl(
            new CefRunContextMenuCallbackImpl(
                base::BindOnce(&CefMenuManager::ExecuteCommandCallback,
                               weak_ptr_factory_.GetWeakPtr())));

        // This reference will be cleared when the callback is executed or
        // the callback object is deleted.
        custom_menu_callback_ = callbackImpl.get();

        if (handler->RunContextMenu(browser_.get(), frame, paramsPtr.get(),
                                    model_.get(), callbackImpl.get())) {
          custom_menu = true;
        } else {
          // Callback should not be executed if the handler returns false.
          DCHECK(custom_menu_callback_);
          custom_menu_callback_ = nullptr;
          callbackImpl->Disconnect();
        }
      }

      // Do not keep references to the parameters in the callback.
      std::ignore = paramsPtr->Detach(nullptr);
      DCHECK(paramsPtr->HasOneRef());
      DCHECK(model_->VerifyRefCount());

      // Menu is empty so notify the client and return.
      if (model_->GetCount() == 0 && !custom_menu) {
        MenuClosed(model_);
        return true;
      }
    }
  }

  if (custom_menu) {
    return true;
  }

  if (!runner_ || !runner_->RunContextMenu(browser_, model_.get(), params_)) {
    LOG(ERROR) << "Default context menu implementation is not available; "
                  "canceling the menu";
    return false;
  }
  return true;
}

void CefMenuManager::CancelContextMenu() {
  if (IsShowingContextMenu()) {
    if (custom_menu_callback_) {
      custom_menu_callback_->Cancel();
    } else if (runner_) {
      runner_->CancelContextMenu();
    }
  }
}

void CefMenuManager::ExecuteCommand(CefRefPtr<CefMenuModelImpl> source,
                                    int command_id,
                                    cef_event_flags_t event_flags) {
  // Give the client a chance to handle the command.
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefContextMenuHandler> handler = client->GetContextMenuHandler();
    if (handler.get()) {
      CefRefPtr<CefContextMenuParamsImpl> paramsPtr(
          new CefContextMenuParamsImpl(&params_));

      bool handled = handler->OnContextMenuCommand(
          browser_.get(), browser_->GetFocusedFrame(), paramsPtr.get(),
          command_id, event_flags);

      // Do not keep references to the parameters in the callback.
      std::ignore = paramsPtr->Detach(nullptr);
      DCHECK(paramsPtr->HasOneRef());

      if (handled) {
        return;
      }
    }
  }

  // Execute the default command handling.
  ExecuteDefaultCommand(command_id);
}

void CefMenuManager::MenuWillShow(CefRefPtr<CefMenuModelImpl> source) {
  // May be called for sub-menus as well.
  if (source.get() != model_.get()) {
    return;
  }

  if (!web_contents()) {
    return;
  }

  // May be called multiple times.
  if (IsShowingContextMenu()) {
    return;
  }

  // Notify the host before showing the context menu.
  web_contents()->SetShowingContextMenu(true);
}

void CefMenuManager::MenuClosed(CefRefPtr<CefMenuModelImpl> source) {
  // May be called for sub-menus as well.
  if (source.get() != model_.get()) {
    return;
  }

  if (!web_contents()) {
    return;
  }

  DCHECK(IsShowingContextMenu());

  // Notify the client.
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefContextMenuHandler> handler = client->GetContextMenuHandler();
    if (handler.get()) {
      handler->OnContextMenuDismissed(browser_.get(),
                                      browser_->GetFocusedFrame());
    }
  }

  // Notify the host after closing the context menu.
  web_contents()->SetShowingContextMenu(false);
  web_contents()->NotifyContextMenuClosed(params_.link_followed);
}

bool CefMenuManager::FormatLabel(CefRefPtr<CefMenuModelImpl> source,
                                 std::u16string& label) {
  if (!runner_) {
    return false;
  }
  return runner_->FormatLabel(label);
}

void CefMenuManager::ExecuteCommandCallback(int command_id,
                                            cef_event_flags_t event_flags) {
  DCHECK(IsShowingContextMenu());
  DCHECK(custom_menu_callback_);
  if (command_id != kInvalidCommandId) {
    ExecuteCommand(model_, command_id, event_flags);
  }
  MenuClosed(model_);
  custom_menu_callback_ = nullptr;
}

void CefMenuManager::CreateDefaultModel() {
  if (!params_.custom_items.empty()) {
    // Custom menu items originating from the renderer process. For example,
    // plugin placeholder menu items.
    for (auto& item : params_.custom_items) {
      auto new_item = item->Clone();
      new_item->action += MENU_ID_CUSTOM_FIRST;
      DCHECK_LE(static_cast<int>(new_item->action), MENU_ID_CUSTOM_LAST);
      model_->AddMenuItem(*new_item);
    }
    return;
  }

  if (params_.is_editable) {
    // Editable node.
    model_->AddItem(MENU_ID_UNDO, GetLabel(IDS_CONTENT_CONTEXT_UNDO));
    model_->AddItem(MENU_ID_REDO, GetLabel(IDS_CONTENT_CONTEXT_REDO));

    model_->AddSeparator();
    model_->AddItem(MENU_ID_CUT, GetLabel(IDS_CONTENT_CONTEXT_CUT));
    model_->AddItem(MENU_ID_COPY, GetLabel(IDS_CONTENT_CONTEXT_COPY));
    model_->AddItem(MENU_ID_PASTE, GetLabel(IDS_CONTENT_CONTEXT_PASTE));
    model_->AddItem(MENU_ID_PASTE_MATCH_STYLE,
                    GetLabel(IDS_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE));

    model_->AddSeparator();
    model_->AddItem(MENU_ID_SELECT_ALL,
                    GetLabel(IDS_CONTENT_CONTEXT_SELECTALL));

    if (!(params_.edit_flags & CM_EDITFLAG_CAN_UNDO)) {
      model_->SetEnabled(MENU_ID_UNDO, false);
    }
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_REDO)) {
      model_->SetEnabled(MENU_ID_REDO, false);
    }
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_CUT)) {
      model_->SetEnabled(MENU_ID_CUT, false);
    }
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_COPY)) {
      model_->SetEnabled(MENU_ID_COPY, false);
    }
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_PASTE)) {
      model_->SetEnabled(MENU_ID_PASTE, false);
      model_->SetEnabled(MENU_ID_PASTE_MATCH_STYLE, false);
    }
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_DELETE)) {
      model_->SetEnabled(MENU_ID_DELETE, false);
    }
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_SELECT_ALL)) {
      model_->SetEnabled(MENU_ID_SELECT_ALL, false);
    }

    if (!params_.misspelled_word.empty()) {
      // Always add a separator before the list of dictionary suggestions or
      // "No spelling suggestions".
      model_->AddSeparator();

      if (!params_.dictionary_suggestions.empty()) {
        for (size_t i = 0; i < params_.dictionary_suggestions.size() &&
                           MENU_ID_SPELLCHECK_SUGGESTION_0 + i <=
                               MENU_ID_SPELLCHECK_SUGGESTION_LAST;
             ++i) {
          model_->AddItem(MENU_ID_SPELLCHECK_SUGGESTION_0 + static_cast<int>(i),
                          params_.dictionary_suggestions[i]);
        }

        // When there are dictionary suggestions add a separator before "Add to
        // dictionary".
        model_->AddSeparator();
      } else {
        model_->AddItem(MENU_ID_NO_SPELLING_SUGGESTIONS,
                        GetLabel(IDS_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS));
        model_->SetEnabled(MENU_ID_NO_SPELLING_SUGGESTIONS, false);
      }

      model_->AddItem(MENU_ID_ADD_TO_DICTIONARY,
                      GetLabel(IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY));
    }
  } else if (!params_.selection_text.empty()) {
    // Something is selected.
    model_->AddItem(MENU_ID_COPY, GetLabel(IDS_CONTENT_CONTEXT_COPY));
  } else if (!params_.page_url.is_empty() || !params_.frame_url.is_empty()) {
    // Page or frame.
    model_->AddItem(MENU_ID_BACK, GetLabel(IDS_CONTENT_CONTEXT_BACK));
    model_->AddItem(MENU_ID_FORWARD, GetLabel(IDS_CONTENT_CONTEXT_FORWARD));

    model_->AddSeparator();
    model_->AddItem(MENU_ID_PRINT, GetLabel(IDS_CONTENT_CONTEXT_PRINT));
    model_->AddItem(MENU_ID_VIEW_SOURCE,
                    GetLabel(IDS_CONTENT_CONTEXT_VIEWPAGESOURCE));

    if (!browser_->CanGoBack()) {
      model_->SetEnabled(MENU_ID_BACK, false);
    }
    if (!browser_->CanGoForward()) {
      model_->SetEnabled(MENU_ID_FORWARD, false);
    }
  }
}

void CefMenuManager::ExecuteDefaultCommand(int command_id) {
  if (IsCustomContextMenuCommand(command_id)) {
    if (web_contents()) {
      web_contents()->ExecuteCustomContextMenuCommand(
          command_id - MENU_ID_CUSTOM_FIRST, params_.link_followed);
    }
    return;
  }

  // If the user chose a replacement word for a misspelling, replace it here.
  if (command_id >= MENU_ID_SPELLCHECK_SUGGESTION_0 &&
      command_id <= MENU_ID_SPELLCHECK_SUGGESTION_LAST) {
    const size_t suggestion_index =
        static_cast<size_t>(command_id) - MENU_ID_SPELLCHECK_SUGGESTION_0;
    if (suggestion_index < params_.dictionary_suggestions.size()) {
      browser_->ReplaceMisspelling(
          params_.dictionary_suggestions[suggestion_index]);
    }
    return;
  }

  switch (command_id) {
    // Navigation.
    case MENU_ID_BACK:
      browser_->GoBack();
      break;
    case MENU_ID_FORWARD:
      browser_->GoForward();
      break;
    case MENU_ID_RELOAD:
      browser_->Reload();
      break;
    case MENU_ID_RELOAD_NOCACHE:
      browser_->ReloadIgnoreCache();
      break;
    case MENU_ID_STOPLOAD:
      browser_->StopLoad();
      break;

    // Editing.
    case MENU_ID_UNDO:
      browser_->GetFocusedFrame()->Undo();
      break;
    case MENU_ID_REDO:
      browser_->GetFocusedFrame()->Redo();
      break;
    case MENU_ID_CUT:
      browser_->GetFocusedFrame()->Cut();
      break;
    case MENU_ID_COPY:
      browser_->GetFocusedFrame()->Copy();
      break;
    case MENU_ID_PASTE:
      browser_->GetFocusedFrame()->Paste();
      break;
    case MENU_ID_PASTE_MATCH_STYLE:
      browser_->GetFocusedFrame()->PasteAndMatchStyle();
      break;
    case MENU_ID_DELETE:
      browser_->GetFocusedFrame()->Delete();
      break;
    case MENU_ID_SELECT_ALL:
      browser_->GetFocusedFrame()->SelectAll();
      break;

    // Miscellaneous.
    case MENU_ID_FIND:
      // TODO(cef): Implement.
      NOTIMPLEMENTED();
      break;
    case MENU_ID_PRINT:
      browser_->Print();
      break;
    case MENU_ID_VIEW_SOURCE:
      browser_->GetFocusedFrame()->ViewSource();
      break;

    // Spell checking.
    case MENU_ID_ADD_TO_DICTIONARY:
      browser_->GetHost()->AddWordToDictionary(params_.misspelled_word);
      break;

    default:
      break;
  }
}

bool CefMenuManager::IsCustomContextMenuCommand(int command_id) {
  // Verify that the command ID is in the correct range.
  if (command_id < MENU_ID_CUSTOM_FIRST || command_id > MENU_ID_CUSTOM_LAST) {
    return false;
  }

  command_id -= MENU_ID_CUSTOM_FIRST;

  // Verify that the specific command ID was passed from the renderer process.
  if (!params_.custom_items.empty()) {
    for (const auto& custom_item : params_.custom_items) {
      if (static_cast<int>(custom_item->action) == command_id) {
        return true;
      }
    }
  }
  return false;
}
