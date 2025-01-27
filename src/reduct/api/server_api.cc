// Copyright 2022 ReductStore
// This Source Code Form is subject to the terms of the Mozilla Public
//    License, v. 2.0. If a copy of the MPL was not distributed with this
//    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "reduct/api/server_api.h"

#include "reduct/auth/token_auth.h"

namespace reduct::api {

using auth::ITokenRepository;
using auth::ParseBearerToken;
using core::Error;
using core::Result;
using storage::IStorage;

Result<HttpRequestReceiver> ServerApi::Alive(const IStorage* storage) { return DefaultReceiver(); }

Result<HttpRequestReceiver> ServerApi::Info(const IStorage* storage) { return SendJson(storage->GetInfo()); }

Result<HttpRequestReceiver> ServerApi::List(const IStorage* storage) { return SendJson(storage->GetList()); }

core::Result<HttpRequestReceiver> ServerApi::Me(const auth::ITokenRepository* token_repo,
                                                std::string_view auth_header) {
  auto [token_value, error] = ParseBearerToken(auth_header);
  if (error) {
    return error;
  }

  return SendJson(token_repo->ValidateToken(token_value));
}

}  // namespace reduct::api
