// Copyright 2022 ReductStore
// This Source Code Form is subject to the terms of the Mozilla Public
//    License, v. 2.0. If a copy of the MPL was not distributed with this
//    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef REDUCT_STORAGE_TOKEN_API_H
#define REDUCT_STORAGE_TOKEN_API_H

#include "reduct/api/common.h"
#include "reduct/storage/storage.h"
#include "rust_part.h"

namespace reduct::api {

class TokenApi {
 public:
  /**
   * POST /tokens/:name
   * @param repository
   * @param name
   * @return
   */
  static core::Result<HttpRequestReceiver> CreateToken(rust_part::TokenRepository& repository,
                                                       const storage::IStorage* storage, std::string_view name);

  /**
   * Token list
   * GET /tokens/
   * @param repository
   * @return
   */
  static core::Result<HttpRequestReceiver> ListTokens(rust_part::TokenRepository& repository);

  /**
   * Token info
   * GET /tokens/:name
   * @param repository
   * @param name
   * @return
   */
  static core::Result<HttpRequestReceiver> GetToken(rust_part::TokenRepository& repository, std::string_view name);

  /**
   * Token delete
   * DELETE /tokens/:name
   * @param repository
   * @param name
   * @return
   */

  static core::Result<HttpRequestReceiver> RemoveToken(rust_part::TokenRepository& repository, std::string_view name);
};

}  // namespace reduct::api
#endif  // REDUCT_STORAGE_TOKEN_API_H
