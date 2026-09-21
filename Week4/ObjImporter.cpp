#include "ObjImporter.h"
#include <sstream>
#include <string_view>
#include <charconv>
#include "FLogManager.h"
#include "StaticMesh.h"

bool FObjImporter::LoadObjModel(FString& FileContent, FStaticMesh& Mesh, TArray<FString>& MaterialFiles)
{
	FObjData Data{};

	if (ParseObjFile(FileContent, Data))
	{
		BuildMeshData(Data, Mesh);
		MaterialFiles = Data.MaterialFiles;
		return true;
	}

	return false;
}

void FObjImporter::BuildMeshData(const FObjData& RawData, FStaticMesh& Mesh)
{
	TMap<FString, int> VertexIndices; // Checking Duplicates

	const TArray<FVector>& Positions = RawData.Positions;
	const TArray<FVector2>& UVs = RawData.UVs;
	const TArray<FVector>& Normals = RawData.Normals;

	for (const FFaceGroupData& FaceGroup : RawData.FaceGroups)
	{
		int SlotIndex = -1;
		if (RawData.MaterialFiles.IsEmpty())
		{
			SlotIndex = 0;
		}
		else {
			for (int i = 0; i < Mesh.Materials.Num(); ++i)
			{
				if (Mesh.Materials[i] == FaceGroup.MaterialName)
				{
					SlotIndex = i;
					break;
				}
			}

			if (SlotIndex == -1)
			{
				Mesh.Materials.Add(FaceGroup.MaterialName);
				SlotIndex = Mesh.Materials.Num() - 1;
			}
		}

		FStaticMeshSection Section;
		Section.StartIndex = Mesh.Indices.Num();
		Section.MaterialSlotIndex = static_cast<uint32>(SlotIndex);

		for (const FFaceData& Face : FaceGroup.Faces)
		{
			int TriangleCount = Face.Vertices.Num() - 2;

			for (int i = 1; i <= TriangleCount; ++i)
			{
				for (int j : { 0, i + 1, i }) // flip winding order
				{
					int VertexIndex = Face.Vertices[j].VertexIndex;
					int UVIndex = Face.Vertices[j].UVIndex;
					int NormalIndex = Face.Vertices[j].NormalIndex;

					FVector Pos = Positions[VertexIndex];
					FVector2 UV = UVIndex >= 0 ? UVs[UVIndex] : FVector2{ 0.0f, 0.0f };
					FVector Norm = NormalIndex >= 0 ? Normals[NormalIndex] : FVector{ 0.0f, 0.0f, 0.0f };

					FVertexSimple Vertex
					{
						Pos.x, Pos.y, Pos.z,
						1.0f, 1.0f, 1.0f, 1.0f,
						UV.X, UV.Y,
						Norm.x, Norm.y, Norm.z
					};
					FString Key = Vertex.GetKey();

					if (!VertexIndices.Contains(Key))
					{
						Mesh.Vertices.Add(Vertex);
						int NewIndex = Mesh.Vertices.Num() - 1;
						Mesh.Indices.Add(NewIndex);
						VertexIndices.Add(Key, NewIndex);
					}
					else
					{
						int ExistingIndex = VertexIndices[Key];
						Mesh.Indices.Add(ExistingIndex);
					}
				}
			}
		}
		Section.IndexCount = Mesh.Indices.Num() - Section.StartIndex;

		if (Section.IndexCount > 0)
		{
			Mesh.Sections.Add(Section);
		}
	}
}

bool FObjImporter::ParseObjFile(FString& FileContent, FObjData& Data)
{
	std::istringstream Stream(FileContent);
	std::string Line;

	FString CurrentGroupName = "default";
	FString CurrentMaterialName = "default";

	if (Stream)
	{
		while (std::getline(Stream, Line))
		{
			// Skip empty or comment line
			if (Line.empty() || Line[0] == '#')
				continue;

			std::istringstream iss(Line);
			std::string prefix;

			if (!(iss >> prefix))
				continue;

			if (prefix == "v")
			{
				float vx, vy, vz;
				iss >> vx >> vy >> vz;
				Data.Positions.Add(PositionToUEBasis({ vx, vy, vz }));
			}
			else if (prefix == "vt")
			{
				float vtcu, vtcv;
				iss >> vtcu >> vtcv;
				Data.UVs.Add(UVToUEBasis({ vtcu, vtcv }));
			}
			else if (prefix == "vn") // normals
			{
				float vnx, vny, vnz;
				iss >> vnx >> vny >> vnz;
				Data.Normals.Add({ vnx, vny, vnz });
			}
			else if (prefix == "g") // group
			{
				std::string GroupName;
				if (iss >> GroupName)
					CurrentGroupName = FString(GroupName);
			}
			else if (prefix == "mtllib") // material library
			{
				std::string MtlFilename;
				if (iss >> MtlFilename)
				{
					Data.MaterialFiles.Add(FString(MtlFilename));
				}
			}
			else if (prefix == "usemtl") // group material
			{
				std::string MaterialName;
				if (iss >> MaterialName)
					CurrentMaterialName = FString(MaterialName);
			}
			else if (prefix == "f") // face
			{
				int LastIndex = Data.FaceGroups.Num() - 1;

				if (Data.FaceGroups.IsEmpty() ||
					Data.FaceGroups[LastIndex].MaterialName != CurrentMaterialName)
				{
					FFaceGroupData NewGroup;
					NewGroup.MaterialName = CurrentMaterialName;
					Data.FaceGroups.Add(NewGroup);
				}

				LastIndex = Data.FaceGroups.Num() - 1;
				ParseFace(iss, Data.FaceGroups[LastIndex].Faces);
			}
		}
	}
	else 
	{
		UE_LOG_ERROR("Failed opening .obj file");
		return false;
	}
	return true;
}

bool FObjImporter::ParseMtlFile(FString& FileContent, TArray<FMaterialData>& Materials)
{
	std::istringstream Stream(FileContent);
	std::string Line;

	if (Stream)
	{
		FString CurrentMaterialName = "";
		FMaterialData Data{};
		while (std::getline(Stream, Line))
		{
			// Skip empty or comment line
			if (Line.empty() || Line[0] == '#')
				continue;

			std::istringstream iss(Line);
			std::string prefix;

			if (!(iss >> prefix))
				continue;

			if (prefix == "newmtl")
			{
				std::string MaterialName;
				if (iss >> MaterialName)
				{
					if (CurrentMaterialName != "")
					{
						Materials.Add(Data);
						Data = FMaterialData{};
					}
					CurrentMaterialName = FString(MaterialName);
					Data.Name = CurrentMaterialName;
				}
			}
			else if (prefix == "Ns")
			{
				float power;
				iss >> power;
				Data.SpecularPower = power;
			}
			else if (prefix == "Ni")
			{
				float density;
				iss >> density;
				Data.OpticalDensity = density;
			}
			else if (prefix == "d")
			{
				float transparency;
				iss >> transparency;
				Data.Transparency = transparency;
			}
			else if (prefix == "Tr")
			{
				float transparency;
				iss >> transparency;
				Data.Transparency = 1.0f - transparency;
			}
			else if (prefix == "Tf")
			{
				float x, y, z;
				iss >> x >> y >> z;
				Data.TransmissionFilter = { x, y, z };
			}
			else if (prefix == "illum")
			{
				int illum;
				iss >> illum;
				Data.IlluminationModel = illum;
			}
			else if (prefix == "Ka")
			{
				float x, y, z;
				iss >> x >> y >> z;
				Data.AmbientColor = { x, y, z };
			}
			else if (prefix == "Kd")
			{
				float x, y, z;
				iss >> x >> y >> z;
				Data.DiffuseColor = { x, y, z };
			}
			else if (prefix == "Ks")
			{
				float x, y, z;
				iss >> x >> y >> z;
				Data.SpecularColor = { x, y, z };
			}
			else if (prefix == "Ke")
			{
				float x, y, z;
				iss >> x >> y >> z;
				Data.EmissiveColor = { x, y, z };
			}
			else if (prefix == "map_Ka")
			{
				std::string filename;
				iss >> filename;
				Data.AmbientColorMapFilename = FString(filename);
			}
			else if (prefix == "map_Kd")
			{
				std::string filename;
				iss >> filename;
				Data.DiffuseColorMapFilename = FString(filename);
			}
			else if (prefix == "map_Ks")
			{
				std::string filename;
				iss >> filename;
				Data.SpecularColorMapFilename = FString(filename);
			}
			else if (prefix == "map_bump" || prefix == "bump")
			{
				std::string filename;
				iss >> filename;
				Data.BumpMapFilename = FString(filename);
			}
		}
		if (CurrentMaterialName != "")
		{
			Materials.Add(Data);
		}
	}
	else 
	{
		UE_LOG_ERROR("Failed opening .mtl file");
		return false;
	}
	return true;
}

void FObjImporter::ParseFace(std::istringstream& iss, TArray<FFaceData>& FaceData)
{
	// TODO: Negative Indices Possible. Handling Needed. 

	TArray<std::string> Strings;
	FFaceData NewFace{};
	std::string VertexString;

	while (iss >> VertexString)
	{
		Strings.Add(VertexString);
	}

	for (std::string s : Strings)
	{
		FVertexData Data{};
		int turn = 0;
		size_t i = 0;

		while (i < s.size())
		{
			if (s[i] >= '0' && s[i] <= '9')
			{
				size_t processed = 0;
				int value = std::stoi(s.substr(i), &processed) - 1;
				switch (turn)
				{
				case 0: // vertex position
					Data.VertexIndex = value;
					break;
				case 1: // texture coordinate
					Data.UVIndex = value;
					break;
				case 2: // normal
					Data.NormalIndex = value;
					break;
				default:
					break;
				}
				i += processed;
			}
			else if (s[i] == '/')
			{
				// check continuous '/'
				// => no texture coordinates
				if (i + 1 < s.size() && s[i + 1] == '/')
				{
					++i;
					++turn;
				}
				++i;
				++turn;
			}
			else {
				++i;
			}
		}
		NewFace.Vertices.Add(Data);
	}
	FaceData.Add(NewFace);
}