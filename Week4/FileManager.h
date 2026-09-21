#pragma once

#include <filesystem>

#include "Core.h"

inline constexpr std::string_view kDefaultRootPath = ".\\";
inline constexpr std::string_view kDefaultAssetsPath = ".\\Assets\\";

class FFileManager
{
public:
	FFileManager();
	FFileManager(std::string_view fileDirPath);
	FFileManager(std::string_view fileDirPath, std::string_view rootPath);

	FString ReadFileToString(std::string_view fileName) const;
	void WriteStringToFile(std::string_view fileName, std::string_view content) const;

	// 파일 탐색기에서 받은 절대 경로용
	FString ReadFileToString(const std::filesystem::path& filePath) const;

	void WriteStringToFile(const std::filesystem::path& filePath,std::string_view content) const;

	// 절대경로를 프로젝트 루트(mRootPath) 기준 상대경로로 바꾼다.
	// 에셋 키처럼 다른 컴퓨터에서도 똑같이 재현돼야 하는 값은 절대경로가 아니라 이걸 써야 한다.
	std::filesystem::path MakeRelativeToRoot(const std::filesystem::path& filePath) const;

private:
	std::filesystem::path mFileDirPath;
	std::filesystem::path mRootPath;

	std::filesystem::path ResolvePath(const std::filesystem::path& filePath) const;

	bool IsUnderRoot(const std::filesystem::path& filePath) const;
	bool IsUnderFileDir(const std::filesystem::path& filePath) const;
};

bool IsUnder(const std::filesystem::path& filePath, const std::filesystem::path& rootPath);
