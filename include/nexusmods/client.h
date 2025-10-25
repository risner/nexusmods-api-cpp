#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

#include "httplib.h"
#include "rapidjson/document.h"

namespace nexusmods {

struct NexusResponse {
  long status;
  std::string body;
  httplib::Headers headers;
};

class Client {
public:
  // v1 enpoint: api.nexusmods.com
  // api_key: personal API key string
  // user_agent: recommended UA string
  Client(const std::string &api_key,
         const std::string &host = "api.nexusmods.com", int port = 443,
         const std::string &user_agent = "nexusmods-cpp/1.0");

  ~Client();

  // Set custom header name if needed (default "apikey")
  void set_api_header_name(const std::string &header_name);

  // Timeout for single request in seconds
  void set_timeout_seconds(int seconds);

  // Low-level GET returning raw response
  std::optional<NexusResponse>
  get(const std::string &path,
      const httplib::Params &params = httplib::Params(),
      const httplib::Headers &extra_headers = httplib::Headers());

  // Convenience JSON parsing wrapper. Returns RapidJSON Document on success,
  // nullopt otherwise.
  std::optional<rapidjson::Document>
  get_json(const std::string &path,
           const httplib::Params &params = httplib::Params(),
           const httplib::Headers &extra_headers = httplib::Headers());

  // --- High level helpers for endpoints you requested ---
  // All return optional Document (parsed JSON) or std::nullopt on failure.

  // Mods
  std::optional<rapidjson::Document>
  get_updated_mods(const std::string &game_domain_name,
                   const httplib::Params &params = httplib::Params());

  std::optional<rapidjson::Document>
  get_mod_changelogs(const std::string &game_domain_name,
                     const std::string &mod_id,
                     const httplib::Params &params = httplib::Params());

  std::optional<rapidjson::Document>
  get_latest_added(const std::string &game_domain_name);
  std::optional<rapidjson::Document>
  get_latest_updated(const std::string &game_domain_name);
  std::optional<rapidjson::Document>
  get_trending(const std::string &game_domain_name);
  std::optional<rapidjson::Document>
  get_mod(const std::string &game_domain_name, const std::string &mod_id);

  std::optional<rapidjson::Document>
  md5_search(const std::string &game_domain_name, const std::string &md5_hash);

  // Mod Files
  std::optional<rapidjson::Document>
  list_mod_files(const std::string &game_domain_name, const std::string &mod_id,
                 const httplib::Params &params = httplib::Params());

  std::optional<rapidjson::Document>
  get_mod_file(const std::string &game_domain_name, const std::string &mod_id,
               const std::string &file_id);

  std::optional<rapidjson::Document>
  get_file_download_link(const std::string &game_domain_name,
                         const std::string &mod_id, const std::string &file_id);

  // Game Info
  std::optional<rapidjson::Document>
  get_game(const std::string &game_domain_name);

  // Set backoff callback for logging sleeps/backoff (signature:
  // seconds_to_sleep)
  void set_backoff_callback(std::function<void(int)> cb);

private:
  httplib::SSLClient *client_;
  std::string api_key_;
  std::string api_header_name_; // default = "apikey"
  std::string user_agent_;
  std::mutex mutex_;
  int timeout_seconds_;
  std::function<void(int)> backoff_cb_;

  // Rate-limit helper
  std::optional<NexusResponse>
  perform_get_with_rate_limit(const std::string &path,
                              const httplib::Params &params,
                              const httplib::Headers &extra_headers);

  httplib::Headers build_auth_headers(const httplib::Headers &extra) const;
};

} // namespace nexusmods
