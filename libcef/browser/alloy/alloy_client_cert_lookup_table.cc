// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "libcef/browser/alloy/alloy_client_cert_lookup_table.h"

#include "base/logging.h"

std::map<std::string, AlloyCert> AlloyClientCertLookupTable::certs_;
std::set<std::string> AlloyClientCertLookupTable::denieds_;

void AlloyClientCertLookupTable::Clear() {
  certs_.clear();
  denieds_.clear();
}

bool AlloyClientCertLookupTable::IsDenied(const std::string& host, int port) {
  std::string host_port = HostAndPort(host, port);
  auto it = denieds_.find(host_port);
  if (it != denieds_.end()) {
    return true;
  }
  return false;
}

void AlloyClientCertLookupTable::Allow(const std::string& host,
                                       int port,
                                       const std::string& private_key,
                                       const std::string& cert_chain) {
  std::string host_port = HostAndPort(host, port);
  AlloyCert cert(private_key, cert_chain);
  certs_[host_port] = cert;
  denieds_.erase(host_port);
}

void AlloyClientCertLookupTable::Deny(const std::string& host, int port) {
  std::string host_port = HostAndPort(host, port);
  certs_.erase(host_port);
  denieds_.insert(host_port);
}

bool AlloyClientCertLookupTable::GetCertData(const std::string& host,
                                             int port,
                                             AlloyCert& cert) {
  std::string host_port = HostAndPort(host, port);
  auto it = certs_.find(host_port);
  if (it != certs_.end()) {
    cert = (*it).second;
    return true;
  }
  return false;
}

std::string AlloyClientCertLookupTable::HostAndPort(const std::string& host,
                                                    int port) {
  return host + ":" + std::to_string(port);
}
