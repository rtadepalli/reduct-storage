// Copyright 2021-2022 ReductStore
// This Source Code Form is subject to the terms of the Mozilla Public
//    License, v. 2.0. If a copy of the MPL was not distributed with this
//    file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
#include <uWebSockets/Loop.h>

#include <csignal>

#include "reduct/api/http_server.h"
#include "reduct/asset/asset_manager.h"
#include "reduct/async/loop.h"
#include "reduct/auth/token_auth.h"
#include "reduct/config.h"
#include "reduct/core/env_variable.h"
#include "reduct/core/logger.h"
#include "reduct/storage/storage.h"

#ifdef WITH_CONSOLE
#include "reduct/console.h"
#endif

using reduct::api::IHttpServer;
using reduct::asset::IAssetManager;
using reduct::async::ILoop;
using reduct::auth::ITokenAuthorization;
using reduct::auth::ITokenRepository;
using reduct::core::EnvVariable;
using reduct::core::Error;
using reduct::core::Logger;
using ReductStorage = reduct::storage::IStorage;

class Loop : public ILoop {
 public:
  void Defer(Task&& task) override { uWS::Loop::get()->defer(std::move(task)); }
};

static bool running = true;
static void SignalHandler(auto signal) { running = false; }

int main() {
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  LOG_INFO("ReductStore {}", reduct::kVersion);

  EnvVariable env;
  auto log_level = env.Get<std::string>("RS_LOG_LEVEL", "INFO");
  auto host = env.Get<std::string>("RS_HOST", "0.0.0.0");
  auto port = env.Get<int>("RS_PORT", 8383);
  auto api_base_path = env.Get<std::string>("RS_API_BASE_PATH", "/");
  auto data_path = env.Get<std::string>("RS_DATA_PATH", "/data");
  auto api_token = env.Get<std::string>("RS_API_TOKEN", "", true);
  auto cert_path = env.Get<std::string>("RS_CERT_PATH", "");
  auto cert_key_path = env.Get<std::string>("RS_CERT_KEY_PATH", "");

  Logger::set_level(log_level);

  LOG_INFO("Configuration: \n {}", env.Print());

  Loop loop;
  ILoop::set_loop(&loop);

#if WITH_CONSOLE
  auto console = IAssetManager::BuildFromZip(reduct::kZippedConsole);
#else
  auto console = IAssetManager::BuildEmpty();
#endif

  IHttpServer::Components components{
      .storage = ReductStorage::Build({.data_path = data_path}),
      .auth = ITokenAuthorization::Build(api_token),
      .token_repository = ITokenRepository::Build({.data_path = data_path, .api_token = api_token}),
      .console = std::move(console),
  };

  auto server = IHttpServer::Build(std::move(components), {
                                                              .host = host,
                                                              .port = port,
                                                              .base_path = api_base_path,
                                                              .cert_path = cert_path,
                                                              .cert_key_path = cert_key_path,
                                                          });
  return server->Run(running);
}
