#include "fauna/repository.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "usage: fauna <command>\n";
    return 1;
  }

  const std::string command = argv[1];

  // ---------------------------------------
  // fauna init
  // ---------------------------------------
  if (command == "init") {
    try {
      const auto current_directory =
          std::filesystem::current_path();

      const auto repository =
          fauna::Repository::init(
              current_directory);

      std::cout
          << "Initialized empty Fauna Repository in "
          << repository.root()
          << '\n';

      return 0;
    } catch (const std::exception &error) {
      std::cerr
          << "fauna: "
          << error.what()
          << '\n';

      return 1;
    }
  }

  // ---------------------------------------
  // fauna status
  // ---------------------------------------
  if (command == "status") {
    try {
      const auto current_directory =
          std::filesystem::current_path();

      const auto repository =
          fauna::Repository::discover(
              current_directory);

      const auto untracked =
          repository.untracked_files();

      if (untracked.empty()) {
        std::cout
            << "nothing to report\n";

        return 0;
      }

      std::cout
          << "Untracked files:\n";

      for (const auto &file : untracked) {
        std::cout
            << "  "
            << file
            << '\n';
      }

      return 0;
    } catch (const std::exception &error) {
      std::cerr
          << "fauna: "
          << error.what()
          << '\n';

      return 1;
    }
  }

  // ---------------------------------------
  // fauna add
  // ---------------------------------------
  if (command == "add") {
    if (argc < 3) {
      std::cerr
          << "usage: fauna add <file>\n";

      return 1;
    }

    try {
      const auto current_directory =
          std::filesystem::current_path();

      auto repository =
          fauna::Repository::discover(
              current_directory);

      repository.add(argv[2]);

      std::cout
          << "Added "
          << argv[2]
          << '\n';

      return 0;
    } catch (const std::exception &error) {
      std::cerr
          << "fauna: "
          << error.what()
          << '\n';

      return 1;
    }
  }

  // ---------------------------------------
  // unknown command
  // ---------------------------------------
  std::cerr
      << "fauna: unknown command '"
      << command
      << "'\n";

  return 1;
}
