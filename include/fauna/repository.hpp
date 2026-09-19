#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fauna {

class Repository {
public:
  static Repository init(
      const std::filesystem::path &path);

  static Repository discover(
      const std::filesystem::path &start);

  const std::filesystem::path &root() const noexcept;

  std::vector<std::filesystem::path>
  untracked_files() const;

  void add(
      const std::filesystem::path &path);

  std::string commit(
      const std::string &message);

private:
  explicit Repository(
      std::filesystem::path root);

  std::filesystem::path root_;
};

} // namespace fauna