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

struct FAssetMetaInfo
{
	// FAssetManager 조회 키. .uasset처럼 프로젝트 내부의 유일한 경로가 들어온다.
	FName AssetName;

	// 사람이 읽는 표시 이름(파일 stem). UAsset::AssetName으로 그대로 넘어간다.
	FName Stem;

	FAssetLoader* AssetLoader = nullptr;

	// LoadAsset이 실제로 읽는 소스. 구워지는 에셋이면 .uasset 파일을 가리킨다.
	FAssetSource* AssetSource = nullptr;

	// 재임포트 판단/실행에만 쓰는 원본 소스(예: obj). 굽기가 필요 없는 에셋이면 nullptr.
	FFileAssetSource* ImportSource = nullptr;

	// 지금 메모리에 로드돼 있으면 그 인스턴스, 아니면 nullptr.
	UAsset* LoadedAsset = nullptr;
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

	// ImportPath(원본 obj 등)를 이미 임포트해서 등록해둔 에셋이 있으면 그 키를 OutAssetName에 담아 true.
	// 없으면 false — 처음 보는 소스라는 뜻이라 새 이름으로 임포트해야 한다.
	bool FindAssetByImportPath(const std::filesystem::path& ImportPath, FName& OutAssetName) const;

	// 굽기(Import) 직후, 새로 만든 엔트리에 원본 소스를 한 번 붙여둔다. 재임포트 때는 다시 부를 필요 없다.
	void SetImportSource(const FName& AssetName, FFileAssetSource* ImportSource);

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
		return AssetMetaInfoMap.Contains(AssetName);
	}

	void UnloadAsset(const FName& AssetName);

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

	// 재임포트로 OldAsset이 통째로 새 인스턴스(NewAsset)로 바뀌었을 때,
	// OldAsset을 직접 들고 있던 참조들을 찾아 NewAsset으로 다시 묶는다.
	void RebindReferences(UAsset* OldAsset, UAsset* NewAsset);

	TMap<FName, FAssetMetaInfo, FNameHasher> AssetMetaInfoMap;

	// 로더는 타입 하나당 인스턴스 하나를 여러 에셋이 공유하므로 별도로 소유 목록을 둔다.
	// 소스는 AssetMetaInfo 하나가 전용으로 소유하므로 별도 목록이 필요 없다.
	TArray<FAssetLoader*> OwnedLoaders;
};
