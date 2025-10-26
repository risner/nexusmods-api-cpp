#include "nexusmods/client.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <thread>

#include "rapidjson/error/en.h"

namespace nexusmods {

using namespace std::chrono_literals;

Client::Client(const std::string &api_key, const std::string &host, int port,
               const std::string &user_agent)
    : api_key_(api_key), api_header_name_("apikey"), user_agent_(user_agent),
      timeout_seconds_(30), backoff_cb_(nullptr) {
  // Create SSL client; httplib's SSLClient needs host and port
  client_ = new httplib::SSLClient(host.c_str(), port);
}

Client::~Client() {
  delete client_;
  client_ = nullptr;
}

void Client::set_api_header_name(const std::string &header_name) {
  std::lock_guard<std::mutex> l(mutex_);
  api_header_name_ = header_name;
}

void Client::set_timeout_seconds(int seconds) {
  std::lock_guard<std::mutex> l(mutex_);
  timeout_seconds_ = seconds;
  if (client_) {
    client_->set_connection_timeout(std::chrono::seconds(timeout_seconds_));
    client_->set_read_timeout(std::chrono::seconds(timeout_seconds_));
    client_->set_write_timeout(std::chrono::seconds(timeout_seconds_));
  }
}

void Client::set_backoff_callback(std::function<void(int)> cb) {
  backoff_cb_ = cb;
}

httplib::Headers
Client::build_auth_headers(const httplib::Headers &extra) const {
  httplib::Headers headers = extra;
  headers.emplace(api_header_name_, api_key_);
  headers.emplace("User-Agent", user_agent_);
  headers.emplace("Accept", "application/json");
  return headers;
}

std::optional<NexusResponse>
Client::perform_get_with_rate_limit(const std::string &path,
                                    const httplib::Params &params,
                                    const httplib::Headers &extra_headers) {
  int attempt = 0;
  int max_attempts = 6;
  int base_backoff_seconds = 1;

  while (attempt < max_attempts) {
    attempt++;

    auto headers = build_auth_headers(extra_headers);

    httplib::Result res;
    if (params.empty()) {
      res = client_->Get(path.c_str(), headers);
    } else {
      res = client_->Get(path.c_str(), params, headers);
    }

    if (!res) {
      int sleep_seconds = base_backoff_seconds * (1 << std::min(attempt, 6));
      if (backoff_cb_)
        backoff_cb_(sleep_seconds);
      std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));
      continue;
    }

    auto &response = *res;

    // Rate-Limit Check (429 Too Many Requests)
    if (response.status == 429) {
      int retry_after = 0;
      auto it = response.headers.find("Retry-After");
      if (it != response.headers.end()) {
        try {
          retry_after = std::stoi(it->second);
        } catch (...) {
          retry_after = base_backoff_seconds * (1 << attempt);
        }
      } else {
        retry_after = base_backoff_seconds * (1 << attempt);
      }
      if (backoff_cb_)
        backoff_cb_(retry_after);
      std::this_thread::sleep_for(std::chrono::seconds(retry_after));
      continue;
    }

    // Check rate-limit related headers (if present)
    // Common headers: X-RateLimit-Remaining, X-RateLimit-Reset
    // If remaining == 0, sleep until reset if available
    {
      auto it = response.headers.find("X-RateLimit-Remaining");
      if (it != response.headers.end() && it->second == "0") {
        int reset_seconds = 0;
        auto it2 = response.headers.find("X-RateLimit-Reset");
        if (it2 != response.headers.end()) {
          try {
            reset_seconds = std::stoi(it2->second);
          } catch (...) {
            reset_seconds = base_backoff_seconds * (1 << attempt);
          }
        } else {
          reset_seconds = base_backoff_seconds * (1 << attempt);
        }
        if (backoff_cb_)
          backoff_cb_(reset_seconds);
        std::this_thread::sleep_for(std::chrono::seconds(reset_seconds));
        continue;
      }
    }

    NexusResponse out;
    out.status = response.status;
    out.body = response.body;
    out.headers = response.headers;
    return out;
  }

  return std::nullopt;
}

std::optional<NexusResponse>
Client::get(const std::string &path, const httplib::Params &params,
            const httplib::Headers &extra_headers) {
  return perform_get_with_rate_limit(path, params, extra_headers);
}

std::optional<rapidjson::Document>
Client::get_json(const std::string &path, const httplib::Params &params,
                 const httplib::Headers &extra_headers) {

  auto error_json = [](int code, const std::string &message,
                       const std::string &path) {
    rapidjson::Document err;
    err.SetObject();
    auto &alloc = err.GetAllocator();
    err.AddMember("code", code, alloc);
    err.AddMember("message", rapidjson::Value(message.c_str(), alloc), alloc);
    err.AddMember("endpoint", rapidjson::Value(path.c_str(), alloc), alloc);
    return err;
  };

  auto r = get(path, params, extra_headers);
  if (!r) {
    // {"code":998,"message":"API error - get() failed"}
    return error_json(998, "[ERROR] HTTP request failed (no response object).",
                      path);
  }
  if (r->status < 200 || r->status >= 300) {
    // failure
    std::ostringstream oss;
    oss << "[ERROR] HTTP request failed with status " << r->status;
    if (!r->body.empty())
      oss << " | Body: " << r->body.substr(0, 300);
    return error_json(997, oss.str(), path);
  }

  rapidjson::Document d;
  rapidjson::ParseResult ok = d.Parse(r->body.c_str(), r->body.size());

  if (!ok) {
    std::ostringstream oss;
    oss << "[ERROR] JSON parse failed: "
        << rapidjson::GetParseError_En(ok.Code()) << " (offset " << ok.Offset()
        << ")";
    return error_json(996, oss.str(), path);
  }

  return d;
}

std::optional<rapidjson::Document>
Client::get_updated_mods(const std::string &game_domain_name,
                         const httplib::Params &params) {
  std::ostringstream path;
  path << "/v1/games/" << game_domain_name << "/mods/updated.json";
  return get_json(path.str(), params);
}

std::optional<rapidjson::Document>
Client::get_mod_changelogs(const std::string &game_domain_name,
                           const std::string &mod_id,
                           const httplib::Params &params) {
  std::ostringstream path;
  path << "/v1/games/" << game_domain_name << "/mods/" << mod_id
       << "/changelogs.json";
  return get_json(path.str(), params);
}

std::optional<rapidjson::Document>
Client::get_latest_added(const std::string &game_domain_name) {
  std::ostringstream path;
  path << "/v1/games/" << game_domain_name << "/mods/latest_added.json";
  return get_json(path.str());
}

std::optional<rapidjson::Document>
Client::get_latest_updated(const std::string &game_domain_name) {
  std::ostringstream path;
  path << "/v1/games/" << game_domain_name << "/mods/latest_updated.json";
  return get_json(path.str());
}

std::optional<rapidjson::Document>
Client::get_trending(const std::string &game_domain_name) {
  std::ostringstream path;
  path << "/v1/games/" << game_domain_name << "/mods/trending.json";
  return get_json(path.str());
}

std::optional<rapidjson::Document>
Client::get_mod(const std::string &game_domain_name,
                const std::string &mod_id) {
  std::ostringstream path;
  path << "/v1/games/" << game_domain_name << "/mods/" << mod_id << ".json";
  return get_json(path.str());
}

std::optional<rapidjson::Document>
Client::md5_search(const std::string &game_domain_name,
                   const std::string &md5_hash) {
  std::ostringstream path;
  path << "/v1/games/" << game_domain_name << "/mods/md5_search/" << md5_hash
       << ".json";
  return get_json(path.str());
}

std::optional<rapidjson::Document>
Client::list_mod_files(const std::string &game_domain_name,
                       const std::string &mod_id,
                       const httplib::Params &params) {
  std::ostringstream path;
  path << "/v1/games/" << game_domain_name << "/mods/" << mod_id
       << "/files.json";
  return get_json(path.str(), params);
}

std::optional<rapidjson::Document>
Client::get_mod_file(const std::string &game_domain_name,
                     const std::string &mod_id, const std::string &file_id) {
  std::ostringstream path;
  path << "/v1/games/" << game_domain_name << "/mods/" << mod_id << "/files/"
       << file_id << ".json";
  return get_json(path.str());
}

std::optional<rapidjson::Document>
Client::get_file_download_link(const std::string &game_domain_name,
                               const std::string &mod_id,
                               const std::string &file_id) {

  // NOTE: Non-premium members must provide the key and expiry from
  // the .nxm link provided by the website. It is recommended for clients
  // to extract them from the nxm link before sending this request. This
  // ensures that all non-premium members must access the website to
  // download through the API.

  // This library requires premium, as there is no support for access to
  // the downloaded .nxm file.

  std::ostringstream path;
  path << "/v1/games/" << game_domain_name << "/mods/" << mod_id << "/files/"
       << file_id << "/download_link.json";
  return get_json(path.str());
}

std::optional<rapidjson::Document>
Client::get_game(const std::string &game_domain_name) {
  std::ostringstream path;
  path << "/v1/games/" << game_domain_name << ".json";
  return get_json(path.str());
}

} // namespace nexusmods
