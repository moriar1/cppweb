#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

struct CsvFileError : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct CommunityNotFound : public std::out_of_range {
  using std::out_of_range::out_of_range;
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
  if (!std::getline(fs, line) || line.empty()) {
    throw CsvFileError("File is empty or unreadable: " + csv_path);
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

int main() {
  try {
    PostData post = read_data("temp.csv");
    std::cout << post.community_id << ' ' << post.title << ' '
              << post.image_file_path << ' ' << post.body << '\n';

  } catch (const CsvFileError &ex) {
    std::cerr << ":: Err: " << ex.what() << '\n';
  } catch (const CommunityNotFound &ex) {
    std::cerr << ":: Err: " << ex.what() << '\n';
  } catch (const std::exception &ex) {
    std::cerr << ":: Err: " << ex.what();
  } catch (...) {
    std::cerr << ":: Err: unknown\n";
  }
}
