// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/password_manager/core/browser/password_store_backend.h"

#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store_built_in_backend.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

std::unique_ptr<PasswordStoreBackend> PasswordStoreBackend::Create(
    std::unique_ptr<LoginDatabase> login_db,
    PrefService* prefs,
    base::RepeatingCallback<bool()> is_syncing_passwords_callback) {
  return std::make_unique<PasswordStoreBuiltInBackend>(std::move(login_db));
}

}  // namespace password_manager
