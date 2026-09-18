#include "fauna/repository.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fauna {

Repository Repository::init(
    const std::filesystem::path &path)
{
  const auto root = path / ".fauna";

  if (std::filesystem::exists(root)) {
    throw std::runtime_error(
        "repository already exists");
  }

  std::filesystem::create_directories(
      root / "objects");

  std::filesystem::create_directories(
      root / "refs" / "heads");

  std::filesystem::create_directories(
      root / "refs" / "tags");

  {
    std::ofstream head(root / "HEAD");

    if (!head) {
      throw std::runtime_error(
          "failed to create HEAD");
    }

    head << "ref: refs/heads/main\n";
  }

  {
    std::ofstream index(root / "index");

    if (!index) {
      throw std::runtime_error(
          "failed to create index");
    }
  }

  return Repository(root);
}

Repository Repository::discover(
    const std::filesystem::path &start)
{
  auto current =
      std::filesystem::absolute(start);

  while (true) {
    const auto fauna_directory =
        current / ".fauna";

    if (std::filesystem::is_directory(
            fauna_directory)) {
      return Repository(fauna_directory);
    }

    const auto parent =
        current.parent_path();

    // We reached the filesystem root.
    if (parent == current) {
      break;
    }

    current = parent;
  }

  throw std::runtime_error(
      "not a Fauna repository");
}

const std::filesystem::path &
Repository::root() const noexcept
{
  return root_;
}

std::vector<std::filesystem::path>
Repository::untracked_files() const
{
  std::vector<std::filesystem::path> files;

  const auto working_tree =
      root_.parent_path();

  // Read files already in the index.
  std::vector<std::filesystem::path>
      indexed_files;

  {
    std::ifstream index(root_ / "index");

    std::string line;

    while (std::getline(index, line)) {
      if (!line.empty()) {
        indexed_files.emplace_back(line);
      }
    }
  }

  for (auto it =
           std::filesystem::recursive_directory_iterator(
               working_tree);
       it !=
       std::filesystem::recursive_directory_iterator();
       ++it) {

    // Never enter .fauna.
    if (it->is_directory() &&
        it->path().filename() == ".fauna") {

      it.disable_recursion_pending();
      continue;
    }

    if (!it->is_regular_file()) {
      continue;
    }

    const auto relative_path =
        std::filesystem::relative(
            it->path(),
            working_tree);

    bool indexed = false;

    for (const auto &indexed_file :
         indexed_files) {

      if (indexed_file == relative_path) {
        indexed = true;
        break;
      }
    }

    if (!indexed) {
      files.push_back(relative_path);
    }
  }

  return files;
}

void Repository::add(
    const std::filesystem::path &path)
{
  const auto working_tree =
      root_.parent_path();

  const auto absolute_path =
      std::filesystem::absolute(path);

  const auto relative_path =
      std::filesystem::relative(
          absolute_path,
          working_tree);

  // Make sure the file is inside
  // the repository.
  if (relative_path.empty() ||
      relative_path.string().starts_with("..")) {
    throw std::runtime_error(
        "path is outside the repository");
  }

  if (!std::filesystem::is_regular_file(
          absolute_path)) {
    throw std::runtime_error(
        "file does not exist");
  }

  // Don't allow Fauna's internal files
  // to be staged.
  if (relative_path.string() == ".fauna" ||
      relative_path.string().starts_with(
          ".fauna/")) {
    throw std::runtime_error(
        "cannot add files inside .fauna");
  }

  // Read the current index.
  std::vector<std::filesystem::path>
      entries;

  {
    std::ifstream index(root_ / "index");

    std::string line;

    while (std::getline(index, line)) {
      if (!line.empty()) {
        entries.emplace_back(line);
      }
    }
  }

  // Don't add the same file twice.
  for (const auto &entry : entries) {
    if (entry == relative_path) {
      return;
    }
  }

  entries.push_back(relative_path);

  // Rewrite the index.
  {
    std::ofstream index(root_ / "index");

    if (!index) {
      throw std::runtime_error(
          "failed to open index");
    }

    for (const auto &entry : entries) {
      index << entry.string() << '\n';
    }
  }
}

Repository::Repository(
    std::filesystem::path root)
    : root_(std::move(root))
{
}

} // namespace fauna
