#include "fauna/repository.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <openssl/evp.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string sha256(const std::string &data)
{
  EVP_MD_CTX *context = EVP_MD_CTX_new();

  if (!context) {
    throw std::runtime_error(
        "failed to create SHA-256 context");
  }

  if (EVP_DigestInit_ex(
          context,
          EVP_sha256(),
          nullptr) != 1) {

    EVP_MD_CTX_free(context);

    throw std::runtime_error(
        "failed to initialize SHA-256");
  }

  if (EVP_DigestUpdate(
          context,
          data.data(),
          data.size()) != 1) {

    EVP_MD_CTX_free(context);

    throw std::runtime_error(
        "failed to hash data");
  }

  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_size = 0;

  if (EVP_DigestFinal_ex(
          context,
          digest,
          &digest_size) != 1) {

    EVP_MD_CTX_free(context);

    throw std::runtime_error(
        "failed to finalize SHA-256");
  }

  EVP_MD_CTX_free(context);

  std::ostringstream result;

  for (unsigned int i = 0;
       i < digest_size;
       ++i) {

    result << std::hex
           << std::setw(2)
           << std::setfill('0')
           << static_cast<int>(digest[i]);
  }

  return result.str();
}

struct IndexEntry {
  std::filesystem::path path;
  std::string hash;
};

std::vector<IndexEntry> read_index(
    const std::filesystem::path &index_path)
{
  std::vector<IndexEntry> entries;

  std::ifstream index(index_path);

  if (!index) {
    return entries;
  }

  std::string line;

  while (std::getline(index, line)) {
    if (line.empty()) {
      continue;
    }

    const auto separator = line.find('\t');

    // Support the old index format:
    //
    // file.txt
    //
    // This lets an existing repository
    // transition to the new index format.
    if (separator == std::string::npos) {
      entries.push_back({
          std::filesystem::path(line),
          ""
      });

      continue;
    }

    const std::string path =
        line.substr(0, separator);

    const std::string hash =
        line.substr(separator + 1);

    if (!path.empty()) {
      entries.push_back({
          std::filesystem::path(path),
          hash
      });
    }
  }

  return entries;
}

void write_index(
    const std::filesystem::path &index_path,
    const std::vector<IndexEntry> &entries)
{
  std::ofstream index(index_path);

  if (!index) {
    throw std::runtime_error(
        "failed to open index");
  }

  for (const auto &entry : entries) {
    index << entry.path.string()
          << '\t'
          << entry.hash
          << '\n';
  }
}

} // namespace

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

  const auto entries =
      read_index(root_ / "index");

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

    for (const auto &entry : entries) {
      if (entry.path == relative_path) {
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

  // Never allow files inside .fauna
  // to be staged.
  if (relative_path.string() == ".fauna" ||
      relative_path.string().starts_with(
          ".fauna/")) {

    throw std::runtime_error(
        "cannot add files inside .fauna");
  }

  // ---------------------------------------
  // Read the file
  // ---------------------------------------

  std::ifstream file(
      absolute_path,
      std::ios::binary);

  if (!file) {
    throw std::runtime_error(
        "failed to open file");
  }

  const std::string contents{
      std::istreambuf_iterator<char>(file),
      std::istreambuf_iterator<char>()
  };

  // ---------------------------------------
  // Hash the file
  // ---------------------------------------

  const std::string hash =
      sha256(contents);

  // ---------------------------------------
  // Store the blob object
  // ---------------------------------------

  const auto object_path =
      root_ / "objects" / hash;

  if (!std::filesystem::exists(object_path)) {
    std::ofstream object(
        object_path,
        std::ios::binary);

    if (!object) {
      throw std::runtime_error(
          "failed to create object");
    }

    object.write(
        contents.data(),
        static_cast<std::streamsize>(
            contents.size()));

    if (!object) {
      throw std::runtime_error(
          "failed to write object");
    }
  }

  // ---------------------------------------
  // Update the index
  // ---------------------------------------

  auto entries =
      read_index(root_ / "index");

  bool found = false;

  for (auto &entry : entries) {
    if (entry.path == relative_path) {
      entry.hash = hash;
      found = true;
      break;
    }
  }

  if (!found) {
    entries.push_back({
        relative_path,
        hash
    });
  }

  write_index(
      root_ / "index",
      entries);
}

std::string Repository::commit(
    const std::string &message)
{
  if (message.empty()) {
    throw std::runtime_error(
        "commit message cannot be empty");
  }

  const auto entries =
      read_index(root_ / "index");

  if (entries.empty()) {
    throw std::runtime_error(
        "nothing to commit");
  }

  // ---------------------------------------
  // Make sure every index entry has
  // a blob hash.
  // ---------------------------------------

  for (const auto &entry : entries) {
    if (entry.hash.empty()) {
      throw std::runtime_error(
          "index contains unstaged entries; "
          "run fauna add again");
    }

    const auto object_path =
        root_ / "objects" / entry.hash;

    if (!std::filesystem::is_regular_file(
            object_path)) {

      throw std::runtime_error(
          "missing object for " +
          entry.path.string());
    }
  }

  // ---------------------------------------
  // Read HEAD
  // ---------------------------------------

  std::ifstream head_file(root_ / "HEAD");

  if (!head_file) {
    throw std::runtime_error(
        "failed to read HEAD");
  }

  std::string head;

  std::getline(head_file, head);

  if (head.empty()) {
    throw std::runtime_error(
        "HEAD is empty");
  }

  std::filesystem::path head_reference;

  if (head.starts_with("ref: ")) {
    head_reference =
        root_ / head.substr(5);
  } else {
    throw std::runtime_error(
        "detached HEAD is not supported yet");
  }

  // ---------------------------------------
  // Read parent commit
  // ---------------------------------------

  std::string parent;

  if (std::filesystem::exists(
          head_reference)) {

    std::ifstream reference(head_reference);

    std::getline(reference, parent);
  }

  // ---------------------------------------
  // Create commit contents
  // ---------------------------------------

  const auto now =
      std::chrono::system_clock::now();

  const auto timestamp =
      std::chrono::duration_cast<
          std::chrono::seconds>(
          now.time_since_epoch())
          .count();

  std::ostringstream commit_data;

  if (!parent.empty()) {
    commit_data
        << "parent "
        << parent
        << '\n';
  }

  commit_data
      << "timestamp "
      << timestamp
      << '\n';

  for (const auto &entry : entries) {
    commit_data
        << "file "
        << entry.hash
        << ' '
        << entry.path.generic_string()
        << '\n';
  }

  commit_data
      << '\n'
      << message
      << '\n';

  const std::string contents =
      commit_data.str();

  // ---------------------------------------
  // Hash the commit
  // ---------------------------------------

  const std::string commit_hash =
      sha256(contents);

  // ---------------------------------------
  // Store the commit object
  // ---------------------------------------

  const auto object_path =
      root_ / "objects" / commit_hash;

  if (!std::filesystem::exists(object_path)) {
    std::ofstream object(
        object_path,
        std::ios::binary);

    if (!object) {
      throw std::runtime_error(
          "failed to create commit object");
    }

    object << contents;

    if (!object) {
      throw std::runtime_error(
          "failed to write commit object");
    }
  }

  // ---------------------------------------
  // Move the current branch
  // ---------------------------------------

  {
    std::ofstream reference(head_reference);

    if (!reference) {
      throw std::runtime_error(
          "failed to update branch");
    }

    reference << commit_hash << '\n';
  }

  return commit_hash;
}

Repository::Repository(
    std::filesystem::path root)
    : root_(std::move(root))
{
}

} // namespace fauna