/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2022. All rights reserved.
 */

#include "libcef/browser/password/oh_password_store_factory.h"

#include "base/containers/contains.h"
#include "chrome/browser/browser_process.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "content/public/browser/browser_context.h"
#include "libcef/browser/alloy/alloy_browser_context.h"
#include "libcef/browser/password/oh_sync_start_util.h"

using password_manager::PasswordStore;

namespace {
bool IsSyncingPasswords() {
  return false;
}
}  // namespace
// static
scoped_refptr<PasswordStore> OhPasswordStoreFactory::GetPasswordStoreForContext(
    content::BrowserContext* context,
    ServiceAccessType access_type) {
  // |profile| gets always redirected to a non-Incognito profile below, so
  // Incognito & IMPLICIT_ACCESS means that incognito browsing session would
  // result in traces in the normal profile without the user knowing it.

  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      context->IsOffTheRecord())
    return nullptr;
  return GetInstance()->GetStoreForContext(context, true);
}

// static
OhPasswordStoreFactory* OhPasswordStoreFactory::GetInstance() {
  return base::Singleton<OhPasswordStoreFactory>::get();
}

OhPasswordStoreFactory::OhPasswordStoreFactory() {}

OhPasswordStoreFactory::~OhPasswordStoreFactory() {}

scoped_refptr<PasswordStore> OhPasswordStoreFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) {
  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabaseForProfileStorage(
          context->GetPath()));

  Profile* profile =
      CefBrowserContext::FromBrowserContext(context)->AsProfile();
  if (!profile) {
    return nullptr;
  }

  if (password_store_)
    return password_store_;

  password_store_ = new password_manager::PasswordStore(
      password_manager::PasswordStoreBackend::Create(
          std::move(login_db), profile->GetPrefs(),
          base::BindRepeating(&IsSyncingPasswords)));

  if (!password_store_->Init(profile->GetPrefs(), nullptr)) {
    // TODO(crbug.com/479725): Remove the LOG once this error is visible in the
    // UI.
    LOG(WARNING) << "Could not initialize password store.";
    return nullptr;
  }
  DCHECK(password_store_);
  return password_store_;
}

scoped_refptr<PasswordStore> OhPasswordStoreFactory::GetStoreForContext(
    void* context,
    bool create) {
  if (!context)
    return nullptr;

  const auto& it = mapping_.find(context);
  if (it != mapping_.end())
    return it->second;

  // Object not found.
  if (!create)
    return nullptr;  // And we're forbidden from creating one.

  scoped_refptr<PasswordStore> store;
  store =
      BuildServiceInstanceFor(static_cast<content::BrowserContext*>(context));

  Associate(context, store);
  return store;
}

void OhPasswordStoreFactory::Associate(
    void* context,
    const scoped_refptr<PasswordStore>& store) {
  DCHECK(!base::Contains(mapping_, context));
  mapping_.insert(std::make_pair(context, store));
}

content::BrowserContext* OhPasswordStoreFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}
