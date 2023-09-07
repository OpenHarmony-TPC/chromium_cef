/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2022. All rights reserved.
 */

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
