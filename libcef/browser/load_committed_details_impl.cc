// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/load_committed_details_impl.h"

#include "content/public/browser/navigation_details.h"
#include "url/gurl.h"

CefLoadCommittedDetailsImpl::CefLoadCommittedDetailsImpl(
    const std::string& current_commit_url,
    NavigationType type,
    bool is_main_frame,
    bool is_same_document,
    bool did_replace_entry)
    : current_commit_url_(current_commit_url),
      type_(type),
      is_main_frame_(is_main_frame),
      is_same_document_(is_same_document),
      did_replace_entry_(did_replace_entry) {}

bool CefLoadCommittedDetailsImpl::IsMainFrame() {
  return is_main_frame_;
}

bool CefLoadCommittedDetailsImpl::IsSameDocument() {
  return is_same_document_;
}

bool CefLoadCommittedDetailsImpl::DidReplaceEntry() {
  return did_replace_entry_;
}

CefLoadCommittedDetails::NavigationType
CefLoadCommittedDetailsImpl::GetNavigationType() {
  return type_;
}

CefString CefLoadCommittedDetailsImpl::GetCurrentURL() {
  return current_commit_url_;
}
