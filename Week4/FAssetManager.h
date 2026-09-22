#pragma once

#include "Core.h"
#include "UAsset.h"
#include "TMap.h"
#include "TArray.h"

#include <typeinfo>
#include <type_traits>
#include <utility>
#include <filesystem>

class FFileAssetSource;
class URenderer;
class FFileManager;

struct FAssetMetaInfo
{
	// FAssetManager 조회 키. .uasset처럼 프로젝트 내부의 유일한 경로가 들어온다.
	FName AssetName;

	// 사람이 읽는 표시 이름(파일 stem). UAsset::AssetName으로 그대로 넘어간다.
	FName Stem;

	FAssetLoader* AssetLoader = nullptr;

	// LoadAsset이 실제로 읽는 소스. 구워지는 에셋이면 .uasset 파일을 가리킨다.
	FAssetSource* AssetSource = nullptr;

	// 지금 메모리에 로드돼 있으면 그 인스턴스, 아니면 nullptr.
	UAsset* LoadedAsset = nullptr;

	// FAssetRegistry의 같은 에셋을 가리키는 영구 식별자. 스캔으로 알게 된 에셋에만 채워진다
	// (이름으로 직접 등록한 런타임 에셋은 굽힌 파일이 없어서 비어 있다).
	FGuid Guid;

	// 로드하면 어떤 클래스가 나오는지. 로드된 적 있으면 그 인스턴스의 실제 클래스로 자동 채워지고,
	// 디스크 스캔으로만 알려진(아직 한 번도 로드 안 한) 엔트리는 ScanBakedAssets가 파일 헤더로 채운다.
	// 드롭다운이 "로드 여부와 무관하게" 클래스로 필터링할 수 있는 건 이 필드 덕분이다.
	const FClassInfo* AssetClass = nullptr;
};

class FAssetManager
{
public:
	FAssetManager() = default;
	~FAssetManager();

	static FAssetManager& Get();

	template <typename TLoader, typename... TArgs>
	void RegisterAsset(const FName& AssetName, FAssetSource* AssetSource, TArgs&&... Args)
	{
		RegisterAssetInternal(AssetName, GetOrCreateLoader<TLoader>(std::forward<TArgs>(Args)...), AssetSource);
	}

	void RegisterAsset(UAsset* Asset);

	// 같은 타입의 로더를 이미 갖고 있으면 그걸 돌려주고, 없으면 Args로 만들어 소유 목록에 넣는다.
	template <typename TLoader, typename... TArgs>
	TLoader* GetOrCreateLoader(TArgs&&... Args)
	{
		static_assert(std::is_base_of_v<FAssetLoader, TLoader>, "TLoader must derive from FAssetLoader.");

		for (FAssetLoader* loader : OwnedLoaders)
		{
			// 파생 타입이 기반 타입으로 잘못 잡히지 않도록 정확히 같은 타입만 재사용한다.
			if (loader && typeid(*loader) == typeid(TLoader))
			{
				return static_cast<TLoader*>(loader);
			}
		}

		TLoader* newLoader = new TLoader(std::forward<TArgs>(Args)...);
		OwnedLoaders.Add(newLoader);

		return newLoader;
	}
	void UnregisterAsset(const FName& AssetName);

	// (원본 추적은 FAssetRegistry로 옮겼다 — 원본 옆 .meta의 GUID가 기준이고,
	//  재임포트 판정은 FAssetRegistry::FindAssetsByImportSource가 한다.)

	// FAssetRegistry가 훑어둔 BakedDir의 에셋 중 아직 등록 안 된 것만 로드 없이 미리 등록해둔다.
	// (에셋 드롭다운이 로드 여부와 무관하게 "존재하는 것"을 보여줄 수 있게 하기 위함.)
	// 디스크 탐색과 헤더 해석은 레지스트리가 하고, 여기서는 클래스 -> 로더 결정만 한다.
	void ScanBakedAssets(const std::filesystem::path& BakedDir, URenderer& Renderer, FFileManager& FileManager);

	// 짧은 이름("SpotLightIcon")으로 실제 조회 키(구워진 .uasset 경로)를 가리키게 한다.
	// 임포트를 거치면 키가 경로가 되는데, 엔진 곳곳이 이름으로 집어가고 있어서 그 조회를 살려둔다.
	// 별칭은 엔트리를 새로 만들지 않으므로 인스턴스가 두 개 생기지 않는다.
	void RegisterAssetAlias(const FName& Alias, const FName& AssetName);

	UAsset* LoadAsset(const FName& AssetName, bool bImport = false);
	UAsset* GetAsset(const FName& AssetName, bool loadIfNotLoaded = false);

	template <typename T>
	T* GetAssetAs(const FName& AssetName, bool loadIfNotLoaded = false)
	{
		UAsset* asset = GetAsset(AssetName, loadIfNotLoaded);
		if (asset)
		{
			return asset->Cast<T>();
		}

		return nullptr;
	}

	bool HasAsset(const FName& AssetName) const
	{
		return AssetMetaInfoMap.Contains(ResolveAlias(AssetName));
	}

	void UnloadAsset(const FName& AssetName);

	static std::filesystem::path MakeUniqueBakedPath(const std::filesystem::path& BakedDir, const FString& PreferredStem);

	template <typename Func>
	void ForEachMetaInfo(Func&& func)
	{
		for (auto& pair : AssetMetaInfoMap)
		{
			func(pair.second);
		}
	}

private:
	void RegisterAssetInternal(const FName& AssetName, FAssetLoader* AssetLoader, FAssetSource* AssetSource);

	// 별칭이면 실제 키로, 아니면 들어온 값 그대로.
	FName ResolveAlias(const FName& AssetName) const
	{
		const FName* Found = AssetNameAliases.Find(AssetName);
		return Found ? *Found : AssetName;
	}

	// 재임포트로 OldAsset이 통째로 새 인스턴스(NewAsset)로 바뀌었을 때,
	// OldAsset을 직접 들고 있던 참조들을 찾아 NewAsset으로 다시 묶는다.
	void RebindReferences(UAsset* OldAsset, UAsset* NewAsset);

	TMap<FName, FAssetMetaInfo, FNameHasher> AssetMetaInfoMap;

	// 짧은 이름 -> 실제 조회 키.
	TMap<FName, FName, FNameHasher> AssetNameAliases;

	// 로더는 타입 하나당 인스턴스 하나를 여러 에셋이 공유하므로 별도로 소유 목록을 둔다.
	// 소스는 AssetMetaInfo 하나가 전용으로 소유하므로 별도 목록이 필요 없다.
	TArray<FAssetLoader*> OwnedLoaders;
};
