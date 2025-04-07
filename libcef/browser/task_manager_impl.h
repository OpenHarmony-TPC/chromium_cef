/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CEF_LIBCEF_BROWSER_TASK_MANAGER_IMPL_H_
#define CEF_LIBCEF_BROWSER_TASK_MANAGER_IMPL_H_
#pragma once

#include "base/memory/raw_ptr.h"
#include "cef/include/cef_task_manager.h"
#include "cef/libcef/browser/thread_util.h"
#include "chrome/browser/task_manager/task_manager_observer.h"

namespace task_manager {
class TaskManagerInterface;
}

class CefTaskManagerImpl : public task_manager::TaskManagerObserver,
                           public CefTaskManager {
 public:
  explicit CefTaskManagerImpl(task_manager::TaskManagerInterface* task_manager);
  ~CefTaskManagerImpl();

  CefTaskManagerImpl(const CefTaskManagerImpl&) = delete;
  CefTaskManagerImpl& operator=(const CefTaskManagerImpl&) = delete;

  // CefTaskManager methods:
  size_t GetTasksCount() override;
  bool GetTaskIdsList(TaskIdList& task_ids) override;
  bool GetTaskInfo(int64_t task_id, CefTaskInfo& info) override;
  bool KillTask(int64_t task_id) override;
  int64_t GetTaskIdForBrowserId(int browser_id) override;

 private:
  bool IsValidTaskId(int64_t task_id) const;

  // task_manager::TaskManagerObserver:
  void OnTaskAdded(int64_t id) override;
  void OnTaskToBeRemoved(int64_t id) override;
  void OnTasksRefreshed(const task_manager::TaskIdList& task_ids) override;

  raw_ptr<task_manager::TaskManagerInterface> task_manager_;
  TaskIdList tasks_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefTaskManagerImpl);
};

#endif  // CEF_LIBCEF_BROWSER_TASK_MANAGER_IMPL_H_
