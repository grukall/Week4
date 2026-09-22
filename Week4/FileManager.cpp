#include <fstream>
#include <sstream>
#include <algorithm>

#include "FileManager.h"
#include "FLogManager.h"

FFileManager::FFileManager()
	: FFileManager(kDefaultAssetsPath, kDefaultRootPath)
{
}

FFileManager::FFileManager(std::string_view fileDirPath)
	: FFileManager(fileDirPath, kDefaultRootPath)
{
}

FFileManager::FFileManager(std::string_view fileDirPath, std::string_view rootPath)
	: mFileDirPath(fileDirPath)
	, mRootPath(rootPath)
{
}

FString FFileManager::ReadFileToString(std::string_view fileName) const
{
    return ReadFileToString(
        std::filesystem::path(std::string(fileName)));
}

void FFileManager::WriteStringToFile(
    std::string_view fileName,
    std::string_view content) const
{
    WriteStringToFile(
        std::filesystem::path(std::string(fileName)),
        content);
}

std::filesystem::path FFileManager::ResolvePath(const std::filesystem::path& requestedPath) const
{
    if (requestedPath.empty())
    {
        return std::filesystem::path();
    }

    std::error_code ec;

    // 1. 이미 절대 경로인 경우
    if (requestedPath.is_absolute())
    {
        auto canon = std::filesystem::weakly_canonical(requestedPath, ec);
        return ec ? requestedPath : canon;
    }

    // 2. 상대 경로이지만 현재 디렉토리(또는 프로젝트 루트) 기준으로 이미 존재하는 경우
    if (std::filesystem::exists(requestedPath, ec))
    {
        auto canon = std::filesystem::weakly_canonical(requestedPath, ec);
        return ec ? requestedPath : canon;
    }

    // 3. mRootPath 기준 경로로 존재하는 경우
    const std::filesystem::path rootBased = mRootPath / requestedPath;
    if (std::filesystem::exists(rootBased, ec))
    {
        auto canon = std::filesystem::weakly_canonical(rootBased, ec);
        return ec ? rootBased : canon;
    }

    // 4. mFileDirPath(기본 ./Assets/) 기준 경로로 존재하는 경우
    const std::filesystem::path fileDirBased = mFileDirPath / requestedPath;
    if (std::filesystem::exists(fileDirBased, ec))
    {
        auto canon = std::filesystem::weakly_canonical(fileDirBased, ec);
        return ec ? fileDirBased : canon;
    }

    // 5. 파일이 아직 디스크에 없거나 쓰기 작업인 경우:
    // requestedPath가 이미 "Assets"로 시작하면 중복해서 mFileDirPath(./Assets/)를 붙이지 않는다.
    std::string pathStr = requestedPath.generic_string();
    if (pathStr.rfind("Assets/", 0) == 0 || pathStr.rfind("Assets\\", 0) == 0 ||
        pathStr.rfind("./Assets/", 0) == 0 || pathStr.rfind(".\\Assets\\", 0) == 0)
    {
        auto canon = std::filesystem::weakly_canonical(rootBased, ec);
        return ec ? rootBased : canon;
    }

    auto canon = std::filesystem::weakly_canonical(fileDirBased, ec);
    return ec ? fileDirBased : canon;
}

FString FFileManager::ReadFileToString(const std::filesystem::path& requestedPath) const
{
    if (requestedPath.empty())
    {
        return FString();
    }

    const std::filesystem::path filePath = ResolvePath(requestedPath);

    std::error_code ec;
    if (!std::filesystem::exists(filePath, ec))
    {
        UE_LOG_ERROR("File does not exist: %s (requested: %s)",
            filePath.string().c_str(), requestedPath.string().c_str());
        return FString();
    }

    std::ifstream fileStream(filePath, std::ios::in | std::ios::binary);

    if (!fileStream.is_open())
    {
        UE_LOG_ERROR("Failed to open file for reading: %s (requested: %s)",
            filePath.string().c_str(), requestedPath.string().c_str());
        return FString();
    }

    std::stringstream buffer;
    buffer << fileStream.rdbuf();

    return FString(buffer.str());
}

void FFileManager::WriteStringToFile(const std::filesystem::path& requestedPath, std::string_view content) const
{
    if (requestedPath.empty())
    {
        return;
    }

    const std::filesystem::path filePath = ResolvePath(requestedPath);

    std::error_code ec;
    std::filesystem::create_directories(filePath.parent_path(), ec);

    std::ofstream fileStream(
        filePath,
        std::ios::out | std::ios::trunc);

    if (!fileStream.is_open())
    {
        UE_LOG_ERROR("Failed to open file for writing: %s (requested: %s)",
            filePath.string().c_str(), requestedPath.string().c_str());
        return;
    }

    fileStream << content;
}

std::filesystem::path FFileManager::MakeRelativeToRoot(const std::filesystem::path& filePath) const
{
    const auto absoluteFile = std::filesystem::weakly_canonical(filePath);
    const auto absoluteRoot = std::filesystem::weakly_canonical(mRootPath);

    return std::filesystem::relative(absoluteFile, absoluteRoot);
}

bool FFileManager::IsUnderRoot(const std::filesystem::path& filePath) const
{
	return IsUnder(filePath, mRootPath);
}

bool FFileManager::IsUnderFileDir(const std::filesystem::path& filePath) const
{
	return IsUnder(filePath, mFileDirPath);
}

bool IsUnder(const std::filesystem::path& targetPath, const std::filesystem::path& basePath)
{
    const auto target =
        std::filesystem::weakly_canonical(targetPath);

    const auto base =
        std::filesystem::weakly_canonical(basePath);

    const auto relative =
        std::filesystem::relative(target, base);

    if (relative.empty())
    {
        return true;
    }

    const auto first = relative.begin();

    return first != relative.end() && *first != L"..";
}
