#pragma once

#include <filesystem>
#include <optional>

class FNativeFileDialog
{
public:
    static std::optional<std::filesystem::path> OpenScene(
        void* ownerWindow,
        const std::filesystem::path& initialDirectory);

    static std::optional<std::filesystem::path> SaveScene(
        void* ownerWindow,
        const std::filesystem::path& initialDirectory);
};