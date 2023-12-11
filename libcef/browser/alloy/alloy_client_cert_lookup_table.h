// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CEF_LIBCEF_BROWSER_ALLOY_ALLOY_CLIENT_CERT_LOOKUP_TABLE_H_
#define CEF_LIBCEF_BROWSER_ALLOY_ALLOY_CLIENT_CERT_LOOKUP_TABLE_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "include/cef_base.h"

class AlloyCert {
 public:
  AlloyCert(const std::string& private_key, const std::string& cert_chain)
      : private_key_(private_key), cert_chain_(cert_chain) {}

  AlloyCert() {}

  ~AlloyCert() = default;

  std::string private_key_;

  std::string cert_chain_;
};

class AlloyClientCertLookupTable : public virtual CefBaseRefCounted {
 public:
  AlloyClientCertLookupTable() {}

  ~AlloyClientCertLookupTable() = default;

  static void Clear();

  static void Allow(const std::string& host,
                    int port,
                    const std::string& private_key,
                    const std::string& cert_chain);

  static void Deny(const std::string& host, int port);

  static bool GetCertData(const std::string& host, int port, AlloyCert& cert);

  static bool IsDenied(const std::string& host, int port);

  static std::string HostAndPort(const std::string& host, int port);

 private:
  static std::map<std::string, AlloyCert> certs_;
  static std::set<std::string> denieds_;

  IMPLEMENT_REFCOUNTING(AlloyClientCertLookupTable);
};

#endif
