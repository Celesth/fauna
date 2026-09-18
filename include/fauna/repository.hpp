#pragma once

#include <filesystem>

namespace fauna {
    class Repository {
        public:
            static Repository init(const std::filesystem::path& path); 
            
            static Repository discover(
                const std::filesystem::path& start
            );
            
            const std::filesystem::path& root() const noexcept;
            
        private:
            explicit Repository(std::filesystem::path root);
            std::filesystem::path root_;
    };
}