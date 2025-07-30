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

class HistoryGetVisitsFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("history.getVisits", HISTORY_GETVISITS)

 protected:
  ~HistoryGetVisitsFunction() override {}

  ResponseAction Run() override;

  // Callback for the history function to provide results.
  void QueryComplete(history::QueryURLResult result);
};

class HistorySearchFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("history.search", HISTORY_SEARCH)

 protected:
  ~HistorySearchFunction() override {}

  ResponseAction Run() override;

  // Callback for the history function to provide results.
  void SearchComplete(history::QueryResults results);

 private:
  static void OnHistorySearched(
      const base::WeakPtr<HistorySearchFunction>& function,
      const NWebExtensionHistoryItems* items);

  bool call_history_search_ = false;
  base::WeakPtrFactory<HistorySearchFunction> weak_ptr_factory_{this};
};

class HistoryAddUrlFunction : public HistoryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("history.addUrl", HISTORY_ADDURL)

 protected:
  ~HistoryAddUrlFunction() override {}

  ResponseAction Run() override;
};

class HistoryDeleteAllFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("history.deleteAll", HISTORY_DELETEALL)

 protected:
  ~HistoryDeleteAllFunction() override {}

  ResponseAction Run() override;

  // Callback for the history service to acknowledge deletion.
  void DeleteComplete();
};

class HistoryDeleteUrlFunction : public HistoryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("history.deleteUrl", HISTORY_DELETEURL)

 protected:
  ~HistoryDeleteUrlFunction() override {}

  ResponseAction Run() override;
};

class HistoryDeleteRangeFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("history.deleteRange", HISTORY_DELETERANGE)

 protected:
  ~HistoryDeleteRangeFunction() override {}

  ResponseAction Run() override;

  // Callback for the history service to acknowledge deletion.
  void DeleteComplete();
};