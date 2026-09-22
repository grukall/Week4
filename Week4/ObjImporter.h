#pragma once

#include "Core.h"
#include "Vector.h"
#include "TArray.h"
#include "TMap.h"

struct FStaticMesh;
struct FMaterialData;

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
private:
	void BuildMeshData(const FObjData& RawData, FStaticMesh& Mesh);
	bool ParseObjFile(FString& FileContent, FObjData& Data);
	void ParseFace(std::istringstream& iss, TArray<FFaceData>& FaceData);
};

static FVector PositionToUEBasis(const FVector& InVector)
{
	return FVector(InVector.x, -InVector.y, InVector.z);
}

static FVector2 UVToUEBasis(const FVector2& InVector)
{
	return FVector2(InVector.X, 1.0f - InVector.Y);
}