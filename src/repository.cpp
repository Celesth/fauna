#include "fauna/repository.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace fauna {

Repository Repository::init(
    const std::filesystem::path& path
)
{
    const auto root = path / ".fauna"; // Check for .fauna

    if (std::filesystem::exists(root)) {
        throw std::runtime_error(
            "repository already exists" // No overwrite
        );
    }

    std::filesystem::create_directories(
        root / "objects"
    ); // Creates objects in .fauna

    std::filesystem::create_directories(
        root / "refs" / "heads"
    ); // Creates refs

    std::filesystem::create_directories(
        root / "refs" / "tags"
    ); // Creates tags

    {
        std::ofstream head(root / "HEAD");

        if (!head) {
            throw std::runtime_error(
                "failed to create HEAD"
            );
        }

        head << "ref: refs/heads/main\n";
    }

    {
        std::ofstream index(root / "index");

        if (!index) {
            throw std::runtime_error(
                "failed to create index"
            );
        }
    }

    return Repository(root);
}

Repository Repository::discover(
    const std::filesystem::path& start
)
{
    auto current = std::filesystem::absolute(start);

    while (true) {
        const auto fauna_directory = current / ".fauna";

        if (std::filesystem::is_directory(fauna_directory)) {
            return Repository(fauna_directory);
        }

        const auto parent = current.parent_path();

        // We reached the filesystem root.
        if (parent == current) {
            break;
        }

        current = parent;
    }

    throw std::runtime_error(
        "not a Fauna repository"
    );
}

Repository::Repository(
    std::filesystem::path root
)
    : root_(std::move(root))
{
}

const std::filesystem::path& Repository::root() const noexcept
{
    return root_;
}

} // namespace fauna

