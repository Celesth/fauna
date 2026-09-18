#include "fauna/repository.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "usage: fauna <command>\n";
        return 1;
    }

    const std::string command = argv[1]; // init

    
    // ---------------------------------------
    // fauna init begins
    // ---------------------------------------
    if (command == "init") { // initialization code
        try {
            const auto current_directory =
                std::filesystem::current_path();

            const auto repository =
                fauna::Repository::init(current_directory);

            std::cout
                << "Initialized empty Fauna Repository in "
                << repository.root()
                << '\n';

            return 0;
        } catch (const std::exception& error) {
            std::cerr
                << "fauna: "
                << error.what()
                << '\n';

            return 1;
        }
    }

    // -----------------------------------
    // fauna status
    // -----------------------------------
    
    if(command == "status"){
        try{
            const auto current_directory = std::filesystem::current_path();
            
            const auto repository = fauna::Repository::discover(current_directory);
            
            std::cout
                <<"ON Fauna Repository at "
                << repository.root()
                <<'\n';
            return 0;
        } catch (const std::exception& error){
            std::cerr
                << "fauna: "
                << error.what()
                <<'\n';
            return 1;
        }
    }
    std::cerr
        << "fauna: unknown command '"
        << command
        << "'\n";

    return 1;
}