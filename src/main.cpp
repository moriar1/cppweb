#include <cpr/cpr.h>
#include <cpr/error.h>
#include <exception>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
using json = nlohmann::json;

struct CsvFileError : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct CommunityNotFound : public std::out_of_range {
  using std::out_of_range::out_of_range;
};

struct HttpError : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct PostData {
  unsigned community_id{};
  std::string title;
  std::string image_file_path;
  std::string body;
};

// TODO: write test
// TODO: read from json
static unsigned get_community_id(const std::string &community) {
  std::clog << "[TRACE] " << __PRETTY_FUNCTION__ << '\n';

  std::unordered_map<std::string, unsigned> com_table{{"aa", 1}};
  if (auto it = com_table.find(community); it != com_table.end()) {
    return it->second; // return found community id
  }
  throw CommunityNotFound("Unknown community: " + community);
}

// TODO: write tests, add trim()
// NOTE: csv fields must not contain semicolons
static PostData read_data(const std::string &csv_path) {
  std::clog << "[TRACE] " << __PRETTY_FUNCTION__ << '\n';

  std::ifstream fs{csv_path};
  if (!fs.is_open()) {
    throw CsvFileError("Failed to open file: " + csv_path);
  }

  std::string line;
  if (!std::getline(fs, line)) { // read header
    throw CsvFileError("File is empty or unreadable: " + csv_path);
  }
  if (!std::getline(fs, line) || line.empty()) { // read first record
    throw CsvFileError("Record is empty: " + csv_path);
  }

  PostData post;
  std::string community_str;

  std::stringstream ss(line);

  bool ok = std::getline(ss, community_str, ';') &&
            std::getline(ss, post.title, ';') &&
            std::getline(ss, post.image_file_path, ';') &&
            std::getline(ss, post.body, ';');
  if (!ok) {
    throw CsvFileError("CSV format error");
  }

  if (community_str.empty()) {
    throw std::runtime_error("Community name is empty.");
  }
  if (post.title.empty()) {
    throw std::runtime_error("Title is empty.");
  }

  post.community_id = get_community_id(community_str);

  return post;
}

static std::string upload_image(std::string &path) {
  // TODO: check if path exists
  cpr::Response r = cpr::Post(cpr::Url{"https://api.imgchest.com/v1/post"},
                              cpr::Header{{"Accept", "application/json"}},
                              cpr::Bearer{"TODO: read from json"},
                              cpr::Multipart{{"images[]", cpr::File{path}}});

  if (r.error) {
    throw HttpError("Failed upload image: Network error: " + r.error.message);
  }
  if (r.status_code != 200) {
    throw HttpError("Failed upload image: API error: " +
                    std::to_string(r.status_code) + " " + r.text);
  }
  return json::parse(r.text)["data"]["images"][0]["link"]; // TODO: better err
}
int main() {
  try {
    PostData post = read_data("temp.csv");
    std::cout << post.community_id << ' ' << post.title << ' '
              << post.image_file_path << ' ' << post.body << '\n';

    if ((post.image_file_path.find("https://", 0, 8)) != 0) { // if not url
      std::string link = upload_image(post.image_file_path);
      std::cout << link << '\n';
    }

  } catch (const std::exception &ex) {
    std::cerr << ":: Err: " << ex.what() << '\n';
  }
}
