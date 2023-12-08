// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CEF_LIBCEF_BROWSER_PASSWORD_OH_PASSWORD_STORE_FACTORY_H_
#define CEF_LIBCEF_BROWSER_PASSWORD_OH_PASSWORD_STORE_FACTORY_H_

#include <string>

#include "base/memory/singleton.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/service_access_type.h"

namespace password_manager {
class PasswordStore;
}

// Singleton that owns all PasswordStores and associates them with
// Profiles.
class OhPasswordStoreFactory {
 public:
  static scoped_refptr<password_manager::PasswordStore>
  GetPasswordStoreForContext(content::BrowserContext* context,
                             ServiceAccessType set);

  static OhPasswordStoreFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<OhPasswordStoreFactory>;

  OhPasswordStoreFactory();
  ~OhPasswordStoreFactory();

  scoped_refptr<password_manager::PasswordStore> BuildServiceInstanceFor(
      content::BrowserContext* context);
  content::BrowserContext* GetBrowserContextToUse(

      content::BrowserContext* context) const;
  scoped_refptr<password_manager::PasswordStore> GetStoreForContext(
      void* context,
      bool create);
  void Associate(void* context,
                 const scoped_refptr<password_manager::PasswordStore>& store);
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context);

  std::map<void*, scoped_refptr<password_manager::PasswordStore>> mapping_;

  scoped_refptr<password_manager::PasswordStore> password_store_;
};

#endif  // CEF_LIBCEF_BROWSER_PASSWORD_OH_PASSWORD_STORE_FACTORY_H_
