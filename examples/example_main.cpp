#include <cstdlib>
#include <iostream>

#include "nexusmods/client.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

using namespace nexusmods;

int main(int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "Usage: example_app <API_KEY> <game_domain_name>\n";
    std::cerr << "Example: example_app MY_API_KEY cyberpunk2077\n";
    return 1;
  }
  std::string api_key = argv[1];
  std::string game = argv[2];

  nexusmods::Client client(api_key);

  // Optional: log backoffs
  client.set_backoff_callback([](int s) {
    std::cerr << "[backoff] sleeping " << s << "s due to rate-limit/network\n";
  });

  auto latest = client.get_latest_added(game);
  if (!latest) {
    std::cerr << "Failed to get latest added mods for game " << game << "\n";
    return 2;
  }

  const rapidjson::Document &d = *latest;
  // Pretty print (simple)
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  d.Accept(writer);
  std::cout << "Latest added for " << game << ":\n";
  std::cout << sb.GetString() << "\n";

  // Example: fetch first mod id if available and request its files
  if (d.IsArray() && !d.Empty() && d[0].HasMember("mod_id")) {
    std::string mod_id = std::to_string(d[0]["mod_id"].GetInt());
    std::cout << "Fetching files for mod_id=" << mod_id << "\n";
    auto files = client.list_mod_files(game, mod_id);
    if (files) {
      rapidjson::StringBuffer sb2;
      rapidjson::Writer<rapidjson::StringBuffer> w2(sb2);
      files->Accept(w2);
      std::cout << sb2.GetString() << "\n";
    } else {
      std::cerr << "Failed to get files for mod " << mod_id << "\n";
    }
  }

  return 0;
}
