/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

class ReadingListAddEntryFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readingList.addEntry", READINGLIST_ADDENTRY)

  ReadingListAddEntryFunction();
  ReadingListAddEntryFunction(const ReadingListAddEntryFunction&) = delete;
  ReadingListAddEntryFunction& operator=(const ReadingListAddEntryFunction&) =
      delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~ReadingListAddEntryFunction() override;

  void DoEntryAdded(const std::string& error);

  static void OnEntryAdded(
      const base::WeakPtr<ReadingListAddEntryFunction>& function,
      const std::string& error);

  base::WeakPtrFactory<ReadingListAddEntryFunction> weak_ptr_factory_{this};
};

class ReadingListRemoveEntryFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readingList.removeEntry", READINGLIST_REMOVEENTRY)

  ReadingListRemoveEntryFunction();
  ReadingListRemoveEntryFunction(const ReadingListRemoveEntryFunction&) =
      delete;
  ReadingListRemoveEntryFunction& operator=(
      const ReadingListRemoveEntryFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~ReadingListRemoveEntryFunction() override;

  void DoEntryRemoved(const std::string& error);

  static void OnEntryRemoved(
      const base::WeakPtr<ReadingListRemoveEntryFunction>& function,
      const std::string& error);

  base::WeakPtrFactory<ReadingListRemoveEntryFunction> weak_ptr_factory_{this};
};

class ReadingListUpdateEntryFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readingList.updateEntry", READINGLIST_UPDATEENTRY)

  ReadingListUpdateEntryFunction();
  ReadingListUpdateEntryFunction(const ReadingListUpdateEntryFunction&) =
      delete;
  ReadingListUpdateEntryFunction& operator=(
      const ReadingListUpdateEntryFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~ReadingListUpdateEntryFunction() override;

  void DoEntryUpdated(const std::string& error);

  static void OnEntryUpdated(
      const base::WeakPtr<ReadingListUpdateEntryFunction>& function,
      const std::string& error);

  base::WeakPtrFactory<ReadingListUpdateEntryFunction> weak_ptr_factory_{this};
};

class ReadingListQueryFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readingList.query", READINGLIST_QUERY)

  ReadingListQueryFunction();
  ReadingListQueryFunction(const ReadingListQueryFunction&) = delete;
  ReadingListQueryFunction& operator=(const ReadingListQueryFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~ReadingListQueryFunction() override;

  void DoEntryQueried(
      const std::string& error,
      const std::vector<OHOS::NWeb::NWebReadingListEntry>& entries);

  static void OnEntryQueried(
      const base::WeakPtr<ReadingListQueryFunction>& function,
      const std::string& error,
      const std::vector<OHOS::NWeb::NWebReadingListEntry>& entries);

  base::WeakPtrFactory<ReadingListQueryFunction> weak_ptr_factory_{this};
};
