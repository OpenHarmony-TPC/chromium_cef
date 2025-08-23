// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/global_config/global_config_prefs.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "components/prefs/pref_service.h"

namespace global_config {

#if BUILDFLAG(IS_ARKWEB_EXT)
const uint32_t kMaxProbability = 10001;
const char kGlobalConfigFeaturesSwitches[] = "global_config.FeaturesSwitches";
std::string kGlobalConfigDataPath = "";

void RegisterGlobalConfigPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kGlobalConfigFeaturesSwitches);
}

bool CheckFeaturesSwitches(const base::Value& item) {
  const std::string* name = item.GetDict().FindString("name");
  const std::string* cmd_line = item.GetDict().FindString("commandline");
  std::optional<int> probability = item.GetDict().FindInt("probability");
  if ((!name || name->empty()) || (!cmd_line || cmd_line->empty())
      || (!probability || !probability.has_value())) {
    LOG(INFO) << "FeaturesSwitches is invalid.";
    return false;
  }

  std::string hash_str = base::Uuid::GenerateRandomV4().AsLowercaseString() + *name;
  size_t hash_value = base::FastHash(hash_str);
  if ((int)(hash_value % kMaxProbability) > probability.value()) {
    LOG(INFO) << "probability is Not Satisfied.";
    return false;
  }
  return true;
}

void SetFeaturesSwitchesToPrefsFile(base::Value::List prefsList) {
  if (!g_browser_process || !(g_browser_process->local_state())) {
    return;
  }
  g_browser_process->local_state()->ClearPref(kGlobalConfigFeaturesSwitches);
  g_browser_process->local_state()->SetList(kGlobalConfigFeaturesSwitches, std::move(prefsList));
  g_browser_process->local_state()->CommitPendingWrite();
}

void ParseFeaturesSwitchesToPrefs() {
  base::FilePath global_config_path(kGlobalConfigDataPath);
  if (!base::PathExists(global_config_path)) {
    LOG(INFO) << "global config path is not exist:" << global_config_path.value();
    return;
  }

  std::string global_config_json;
  bool res = base::ReadFileToString(global_config_path, &global_config_json);
  if (!res) {
    LOG(WARNING) << "read global config file failed.";
    return;
  }

  std::optional<base::Value> global_config_value = base::JSONReader::Read(global_config_json);
  if (!global_config_value || !global_config_value->is_dict()) {
    LOG(WARNING) << "Invalid JSON.";
    return;
  }

  const base::Value::Dict* data_dict = global_config_value->GetDict().FindDict("data");
  if (!data_dict) {
    LOG(WARNING) << "Missing 'data' field in JSON.";
    return;
  }

  const base::Value::Dict* body_dict = data_dict->FindDict("body");
  if (!body_dict) {
    LOG(WARNING) << "Missing 'body' field in JSON.";
    return;
  }

  const base::Value::Dict* switches_dict = body_dict->FindDict("FeaturesSwitches");
  if (!switches_dict) {
    LOG(WARNING) << "Missing 'FeaturesSwitches' field in JSON.";
    return;
  }

  const base::Value::List* switches_list = switches_dict->FindList("Switches");
  if (!switches_list) {
    LOG(WARNING) << "Missing 'Switches' list.";
    return;
  }

  base::Value::List prefs_list;
  for (const auto& item : *switches_list) {
    if (!item.is_dict()) {
      continue;
    }

    if (CheckFeaturesSwitches(item)) {
      prefs_list.Append(item.Clone());
    }
  }

  SetFeaturesSwitchesToPrefsFile(std::move(prefs_list));
}

void OnGlobalConfigResult(const std::string& path) {
  if (!path.empty()) {
    kGlobalConfigDataPath = path;
  }
  ParseFeaturesSwitchesToPrefs();
}
#endif

} // global_config
