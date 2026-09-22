#include "FAssetRegistry.h"

#include <cctype>
#include <fstream>
#include <string>

#include "Archive.h"
#include "FileManager.h"
#include "FLogManager.h"
#include "UAsset.h"

FAssetRegistry& FAssetRegistry::Get()
{
	static FAssetRegistry Instance;
	return Instance;
}

bool FAssetRegistry::ReadAssetHeader(const std::filesystem::path& Path, FAssetFileHeader& OutHeader)
{
	std::error_code ErrorCode;
	// 매직 + 버전조차 안 들어가는 파일은 열어볼 것도 없다. 잘린 파일을 읽다가
	// 쓰레기 값을 문자열 길이로 해석하는 사고를 여기서 막는다.
	if (std::filesystem::file_size(Path, ErrorCode) < sizeof(uint32) * 2 || ErrorCode)
	{
		return false;
	}

	FArchiveFileReader Reader(Path);
	if (!Reader.IsValid())
	{
		return false;
	}

	uint32 Magic = 0;
	uint32 Version = 0;
	Reader << Magic;
	Reader << Version;

	if (Magic != UAsset::AssetFileMagic)
	{
		return false;
	}

	if (Version > UAsset::AssetFileVersion)
	{
		UE_LOG_WARN("[AssetRegistry] newer asset version (%u > %u), skip: %s",
			Version, UAsset::AssetFileVersion, Path.string().c_str());
		return false;
	}

	OutHeader.Version = Version;
	Reader << OutHeader.ClassName;
	Reader << OutHeader.Guid;
	Reader << OutHeader.AssetName;

	// 버전 2 이하는 이 자리에 원본 경로 문자열이 있었다. 지금 포맷으로 읽으면 어긋나므로
	// 건드리지 않는다 — 헤더 앞부분(GUID까지)은 같아서, 다시 구울 때 GUID는 물려받을 수 있다.
	if (Version >= 3)
	{
		Reader << OutHeader.ImportSourceGuid;
	}

	return !OutHeader.ClassName.empty();
}

bool FAssetRegistry::IsBakedFileCurrent(const std::filesystem::path& Path)
{
	FAssetFileHeader Header;
	if (!ReadAssetHeader(Path, Header))
	{
		return false;
	}

	return Header.Version == UAsset::AssetFileVersion;
}

std::filesystem::path FAssetRegistry::MakeMetaPath(const std::filesystem::path& SourcePath)
{
	// cat.png -> cat.png.meta. 확장자를 바꾸지 않고 덧붙여야 cat.png와 cat.obj가 안 부딪힌다.
	std::filesystem::path MetaPath = SourcePath;
	MetaPath += ".meta";

	return MetaPath;
}

FString FAssetRegistry::MakePathKey(const std::filesystem::path& Path, FFileManager& FileManager) const
{
	std::string Key = FileManager.MakeRelativeToRoot(Path).generic_string();

	// Windows는 대소문자를 구분하지 않는다. 안 맞추면 같은 파일이 두 번 등록된다.
	for (char& Character : Key)
	{
		Character = static_cast<char>(std::tolower(static_cast<unsigned char>(Character)));
	}

	return FString(Key);
}

void FAssetRegistry::Register(const FGuid& Guid, const std::filesystem::path& Path, const FString& ClassName, bool bIsSourceFile, FFileManager& FileManager)
{
	if (!Guid.IsValid())
	{
		UE_LOG_WARN("[AssetRegistry] Register: invalid guid, skip: %s", Path.string().c_str());
		return;
	}

	const FString PathKey = MakePathKey(Path, FileManager);

	if (FAssetRegistryEntry* Existing = Entries.Find(Guid))
	{
		const FString ExistingKey = MakePathKey(Existing->Path, FileManager);
		if (ExistingKey.Equals(PathKey))
		{
			return;
		}

		// 같은 GUID가 두 파일에 있다 = 둘 중 하나로 이동했거나, 파일을 복사했거나.
		// 이전 경로에 파일이 남아있지 않으면 이동으로 보고 위치만 갱신한다. 참조는 GUID라 무사하다.
		if (!std::filesystem::exists(Existing->Path))
		{
			UE_LOG("[AssetRegistry] moved: guid=%s %s -> %s",
				Guid.ToString().CStr(), Existing->Path.string().c_str(), Path.string().c_str());

			PathToGuid.Remove(ExistingKey);
			Existing->Path = Path;
			PathToGuid.Add(PathKey, Guid);
			return;
		}

		// 둘 다 실재한다 = 복사본. 둘 중 뭘 가리켜야 할지 알 수 없으므로 먼저 찾은 쪽을 유지한다.
		// (복사본에 새 GUID를 발급하려면 그 .uasset을 통째로 다시 저장해야 해서, 굽기 쪽 일이다.)
		UE_LOG_WARN("[AssetRegistry] duplicated guid=%s: keep %s, ignore %s",
			Guid.ToString().CStr(), Existing->Path.string().c_str(), Path.string().c_str());
		return;
	}

	FAssetRegistryEntry Entry;
	Entry.Guid = Guid;
	Entry.ClassName = ClassName;
	Entry.Path = Path;
	Entry.bIsSourceFile = bIsSourceFile;

	Entries.Add(Guid, Entry);
	PathToGuid.Add(PathKey, Guid);
}

void FAssetRegistry::SetImportSource(const FGuid& AssetGuid, const FGuid& ImportSourceGuid)
{
	if (!AssetGuid.IsValid() || !ImportSourceGuid.IsValid())
	{
		return;
	}

	FAssetRegistryEntry* Entry = Entries.Find(AssetGuid);
	if (Entry)
	{
		Entry->ImportSourceGuid = ImportSourceGuid;
	}

	TArray<FGuid>& Assets = ImportSourceToAssets[ImportSourceGuid];
	for (const FGuid& Existing : Assets)
	{
		if (Existing == AssetGuid)
		{
			return;
		}
	}

	Assets.Add(AssetGuid);
}

bool FAssetRegistry::FindAssetsByImportSource(const FGuid& ImportSourceGuid, TArray<FGuid>& OutAssetGuids) const
{
	const TArray<FGuid>* Found = ImportSourceToAssets.Find(ImportSourceGuid);
	if (!Found || Found->Num() == 0)
	{
		return false;
	}

	OutAssetGuids = *Found;
	return true;
}

void FAssetRegistry::Unregister(const FGuid& Guid)
{
	FAssetRegistryEntry* Entry = Entries.Find(Guid);
	if (!Entry)
	{
		return;
	}

	// 경로 인덱스는 등록 때 쓴 키로 지워야 한다. FileManager 없이 지우려고
	// 경로를 다시 정규화하면 어긋날 수 있어서, 값으로 찾아 지운다.
	for (const auto& Pair : PathToGuid)
	{
		if (Pair.second == Guid)
		{
			PathToGuid.Remove(Pair.first);
			break;
		}
	}

	// 원본 역인덱스에서도 빼준다. 안 빼면 지워진 에셋이 재임포트 판정에 계속 잡힌다.
	if (Entry->ImportSourceGuid.IsValid())
	{
		if (TArray<FGuid>* Assets = ImportSourceToAssets.Find(Entry->ImportSourceGuid))
		{
			for (uint32 i = 0; i < Assets->Num(); ++i)
			{
				if ((*Assets)[i] == Guid)
				{
					Assets->RemoveAt(i, 1);
					break;
				}
			}
		}
	}

	Entries.Remove(Guid);
}

const FAssetRegistryEntry* FAssetRegistry::Find(const FGuid& Guid) const
{
	return Entries.Find(Guid);
}

bool FAssetRegistry::FindPath(const FGuid& Guid, std::filesystem::path& OutPath) const
{
	const FAssetRegistryEntry* Entry = Entries.Find(Guid);
	if (!Entry)
	{
		return false;
	}

	OutPath = Entry->Path;
	return true;
}

bool FAssetRegistry::FindGuidByPath(const std::filesystem::path& Path, FGuid& OutGuid, FFileManager& FileManager) const
{
	const FGuid* Found = PathToGuid.Find(MakePathKey(Path, FileManager));
	if (!Found)
	{
		return false;
	}

	OutGuid = *Found;
	return true;
}

bool FAssetRegistry::FindGuidByPath(const std::filesystem::path& Path, FGuid& OutGuid) const
{
	const std::filesystem::path Normalized = std::filesystem::weakly_canonical(Path);

	for (const auto& Pair : Entries)
	{
		if (std::filesystem::weakly_canonical(Pair.second.Path) == Normalized)
		{
			OutGuid = Pair.second.Guid;
			return true;
		}
	}

	return false;
}

FGuid FAssetRegistry::AcquireGuidForBake(const std::filesystem::path& BakedPath) const
{
	// 같은 자리에 이미 구워진 게 있으면 그 GUID를 그대로 쓴다 — 재임포트할 때마다 새로
	// 발급해버리면 이 에셋을 참조하던 모든 게 끊어진다. 이 재사용이 GUID 설계의 핵심이다.
	FAssetFileHeader Header;
	if (std::filesystem::exists(BakedPath) && ReadAssetHeader(BakedPath, Header) && Header.Guid.IsValid())
	{
		return Header.Guid;
	}

	return FGuid::NewGuid();
}

FGuid FAssetRegistry::GetOrCreateSourceGuid(const std::filesystem::path& SourcePath, FFileManager& FileManager)
{
	const std::filesystem::path MetaPath = MakeMetaPath(SourcePath);

	if (std::filesystem::exists(MetaPath))
	{
		std::ifstream MetaStream(MetaPath);
		std::string Line;

		if (MetaStream && std::getline(MetaStream, Line))
		{
			FGuid Parsed;
			if (FGuid::Parse(FString(Line), Parsed))
			{
				Register(Parsed, SourcePath, FString(""), true, FileManager);
				return Parsed;
			}
		}

		UE_LOG_WARN("[AssetRegistry] unreadable meta, reissuing: %s", MetaPath.string().c_str());
	}

	const FGuid NewGuid = FGuid::NewGuid();

	std::ofstream MetaStream(MetaPath, std::ios::out | std::ios::trunc);
	if (!MetaStream)
	{
		// 원본이 읽기 전용 위치에 있으면 여기로 온다. GUID는 돌려주되 등록은 하지 않는다 —
		// 다음 실행에서 또 새 값이 나오므로, 이 GUID를 참조로 저장하면 안 된다.
		UE_LOG_WARN("[AssetRegistry] failed to write meta, guid is not persistent: %s", MetaPath.string().c_str());
		return NewGuid;
	}

	MetaStream << NewGuid.ToString().CStr() << "\n";
	MetaStream.close();

	Register(NewGuid, SourcePath, FString(""), true, FileManager);

	UE_LOG("[AssetRegistry] source guid issued: guid=%s path=%s",
		NewGuid.ToString().CStr(), SourcePath.string().c_str());

	return NewGuid;
}

void FAssetRegistry::ScanDirectory(const std::filesystem::path& Directory, FFileManager& FileManager)
{
	if (!std::filesystem::exists(Directory))
	{
		return;
	}

	uint32 ScannedCount = 0;

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(Directory))
	{
		if (!Entry.is_regular_file() || Entry.path().extension() != ".uasset")
		{
			continue;
		}

		FAssetFileHeader Header;
		if (!ReadAssetHeader(Entry.path(), Header))
		{
			UE_LOG_WARN("[AssetRegistry] not a valid asset file, skip: %s", Entry.path().string().c_str());
			continue;
		}

		if (!Header.Guid.IsValid())
		{
			// GUID가 없는 건 이 기능이 들어오기 전에 구운 파일이다. 다시 구우면 발급된다.
			UE_LOG_WARN("[AssetRegistry] no guid (bake it again), skip: %s", Entry.path().string().c_str());
			continue;
		}

		// 헤더는 읽혔지만 본문 레이아웃이 다른 옛 파일이다. 등록해두면 로드할 때 엉뚱하게
		// 해석되므로 건너뛴다 — 원본을 다시 임포트하면 지금 포맷으로 다시 구워진다.
		if (Header.Version != UAsset::AssetFileVersion)
		{
			UE_LOG_WARN("[AssetRegistry] old asset version %u (expected %u), needs re-bake: %s",
				Header.Version, UAsset::AssetFileVersion, Entry.path().string().c_str());
			continue;
		}

		Register(Header.Guid, Entry.path(), Header.ClassName, false, FileManager);

		// 원본과의 연결은 헤더에만 있다. 여기서 역인덱스를 채워야 재시작 후에도
		// "이 obj는 이미 임포트했다"를 알 수 있다.
		SetImportSource(Header.Guid, Header.ImportSourceGuid);

		++ScannedCount;
	}

	UE_LOG("[AssetRegistry] Scan: dir=%s files=%u total=%u",
		Directory.string().c_str(), ScannedCount, Entries.Num());
}
