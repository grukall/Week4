#pragma once

#include <filesystem>

#include "Core.h"
#include "FGuid.h"
#include "FName.h"
#include "TArray.h"
#include "TMap.h"

class FFileManager;

// .uasset 맨 앞에 UAsset::Serialize가 적어두는 고정 헤더.
// 본문(정점/머티리얼 등)은 건드리지 않는다 — 레지스트리는 "이게 무슨 클래스고, GUID가 뭐고,
// 원본이 어디였냐"만 알면 되지 오브젝트를 복원할 필요가 없다.
struct FAssetFileHeader
{
	uint32 Version = 0;
	FString ClassName;
	FGuid Guid;
	FName AssetName;
	FGuid ImportSourceGuid;	// 구울 때 쓴 원본(obj/mtl)의 GUID (있다면)
};

struct FAssetRegistryEntry
{
	FGuid Guid;

	// .uasset이면 헤더에서 읽은 클래스 이름, 원본 파일(png/obj/mtl)이면 비어 있다.
	FString ClassName;

	// 이 GUID가 "지금" 있는 위치. 파일을 옮기면 다음 스캔에서 갱신된다 — ID가 아니다.
	std::filesystem::path Path;

	// 이 에셋을 구울 때 쓴 원본(obj/mtl)의 GUID. 원본 옆 .meta에 적힌 값과 같다.
	FGuid ImportSourceGuid;

	// .meta 사이드카로 GUID를 관리하는 원본 파일인지(= 우리가 굽지 않는 파일인지).
	bool bIsSourceFile = false;
};

// GUID와 현재 경로의 대응만 책임지는 디스크 관점의 인덱스. 에셋을 로드하지 않는다.
//
// FAssetManager와의 분담:
//   FAssetRegistry - 디스크에 뭐가 있고 그게 무슨 GUID인가 (로드 안 함)
//   FAssetManager  - 그 에셋의 인스턴스를 만들고 수명을 관리한다 (메모리)
//
// 참조 그래프(GUID -> 참조하는 GUID들)도 이 클래스로 들어올 자리다.
class FAssetRegistry
{
public:
	static FAssetRegistry& Get();

	// Directory 밑의 .uasset을 전부 훑어 헤더만 읽고 GUID -> 경로 인덱스를 채운다.
	// 이미 등록된 GUID가 다른 경로에서 발견되면 파일이 이동/리네임된 것으로 보고 경로만 갱신한다
	// — 참조는 GUID로 저장돼 있으므로 이 한 줄로 복구가 끝난다.
	void ScanDirectory(const std::filesystem::path& Directory, FFileManager& FileManager);

	// 굽기 직전에 쓸 GUID를 정한다. 같은 자리에 이미 .uasset이 있으면 그 GUID를 재사용하고
	// (재굽기/재임포트), 처음 굽는 자리면 새로 발급한다.
	FGuid AcquireGuidForBake(const std::filesystem::path& BakedPath) const;

	// 우리가 굽지 않는 원본 파일(png/obj/mtl)의 GUID. 옆에 <파일명>.meta를 두고 거기 적어둔다.
	// 원본 바이트는 건드리지 않으므로 어떤 포맷에도 쓸 수 있다.
	FGuid GetOrCreateSourceGuid(const std::filesystem::path& SourcePath, FFileManager& FileManager);

	void Register(const FGuid& Guid, const std::filesystem::path& Path, const FString& ClassName, bool bIsSourceFile, FFileManager& FileManager);
	void Unregister(const FGuid& Guid);

	// "이 에셋은 저 원본에서 나왔다"를 기록한다. 굽고 난 직후에 한 번 불러주면 된다.
	void SetImportSource(const FGuid& AssetGuid, const FGuid& ImportSourceGuid);

	// 그 원본에서 나온 에셋들. 재임포트인지 판단할 때 쓴다 — mtl 하나에서 머티리얼이 여러 개
	// 나오므로 1:N이다. 하나도 없으면 false.
	bool FindAssetsByImportSource(const FGuid& ImportSourceGuid, TArray<FGuid>& OutAssetGuids) const;

	const FAssetRegistryEntry* Find(const FGuid& Guid) const;

	// GUID로 현재 위치를 묻는다. 에셋을 여는 쪽은 경로를 저장해두지 말고 매번 이걸 거쳐야 한다.
	bool FindPath(const FGuid& Guid, std::filesystem::path& OutPath) const;
	bool FindGuidByPath(const std::filesystem::path& Path, FGuid& OutGuid, FFileManager& FileManager) const;

	// FFileManager가 없는 쪽(썸네일 매니저 등)에서 쓰는 버전. 정규화 키 대신 경로를 직접 비교하므로
	// 선형 탐색이지만, 한 번 찾으면 캐시되는 용도에만 쓴다.
	bool FindGuidByPath(const std::filesystem::path& Path, FGuid& OutGuid) const;

	// 매직을 검사하고 헤더만 읽는다. 우리 포맷이 아니거나 더 나중 버전이면 false.
	// 옛 버전도 읽어준다 — 헤더 부분은 그대로라서, 다시 굽더라도 GUID는 물려받을 수 있어야 한다.
	static bool ReadAssetHeader(const std::filesystem::path& Path, FAssetFileHeader& OutHeader);

	// 본문까지 지금 코드로 읽을 수 있는 파일인지. 버전이 다르면 본문 레이아웃이 달라서
	// 그대로 읽으면 안 되고 다시 구워야 한다.
	static bool IsBakedFileCurrent(const std::filesystem::path& Path);

	static std::filesystem::path MakeMetaPath(const std::filesystem::path& SourcePath);

	uint32 Num() const { return Entries.Num(); }

	template <typename Func>
	void ForEachEntry(Func&& Function) const
	{
		for (const auto& Pair : Entries)
		{
			Function(Pair.second);
		}
	}

private:
	// 경로 비교용 정규화 키. 루트 기준 상대경로 + 슬래시 통일 + 소문자.
	// (Windows는 대소문자를 구분하지 않아서, 안 맞추면 같은 파일이 두 번 등록된다.)
	FString MakePathKey(const std::filesystem::path& Path, FFileManager& FileManager) const;

	TMap<FGuid, FAssetRegistryEntry, FGuidHasher> Entries;
	TMap<FString, FGuid> PathToGuid;

	// 원본 GUID -> 그 원본에서 구워진 에셋 GUID들. 스캔할 때 헤더를 읽어 채운다.
	TMap<FGuid, TArray<FGuid>, FGuidHasher> ImportSourceToAssets;
};
