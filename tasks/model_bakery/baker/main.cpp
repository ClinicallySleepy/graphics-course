#include "Baker.hpp"
#include <filesystem>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        spdlog::error("Model file is required\n");
        return 1;
    }

    std::filesystem::path filename = std::filesystem::path(argv[1]);
    spdlog::info("Baking scene from file: {}", filename.string());

    Baker baker;
    baker.bakeScene(filename);

    return 0;
}
