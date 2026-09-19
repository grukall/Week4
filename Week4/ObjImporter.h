#pragma once

#include "Core.h"
#include "Vector.h"
#include "TArray.h"
#include "TMap.h"

struct FStaticMesh;
struct FMaterialData;

struct FNormalVertex
{
	FVector Pos;
	FVector2 UV;
	FVector Normal;

	const FString GetKey() const
	{
		char Buffer[128];
		std::snprintf
		(
			Buffer,
			sizeof(Buffer),
			"%.4f,%.4f,%.4f|%.4f,%.4f|%.4f,%.4f,%.4f",
			Pos.x, Pos.y, Pos.z,
			UV.X, UV.Y,
			Normal.x, Normal.y, Normal.z
		);
		return FString(Buffer);
	}
};

struct FObjImporter
{
	struct FVertexData
	{
		int32 VertexIndex = -1;
		int32 UVIndex = -1;
		int32 NormalIndex = -1;
	};

	struct FFaceData
	{
		TArray<FVertexData> Vertices;
	};

	struct FFaceGroupData
	{
		FString MaterialName;
		TArray<FFaceData> Faces;
	};

	struct FObjData
	{
		TArray<FVector> Positions;
		TArray<FVector2> UVs;
		TArray<FVector> Normals;
		TArray<FFaceGroupData> FaceGroups;
		TArray<FString> MaterialFiles;
	};

	bool LoadObjModel(FString& FileContent, FStaticMesh& Mesh, TArray<FString>& MaterialFiles);
	bool ParseMtlFile(FString& FileContent, TArray<FMaterialData>& Materials);
private:
	void BuildMeshData(const FObjData& RawData, FStaticMesh& Mesh);
	bool ParseObjFile(FString& FileContent, FObjData& Data);
	void ParseFace(std::istringstream& iss, TArray<FFaceData>& FaceData);
};

static FVector PositionToUEBasis(const FVector& InVector)
{
	// assume right handed system, y up coming.
	return { -InVector.z, InVector.x, InVector.y };

	// Unreal Engine code
	//return FVector(InVector.x, -InVector.y, InVector.z);
}

static FVector2 UVToUEBasis(const FVector2& InVector)
{
	return FVector2(InVector.X, 1.0f - InVector.Y);
}