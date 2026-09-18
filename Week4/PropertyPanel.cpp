#include "imgui_internal.h"
#include "PropertyPanel.h"
#include "core.h"
#include "Transform.h"
#include "UAsset.h"
#include "UObjectIterator.h"
#include "FLogManager.h"
#include "Material.h"
#include "UStaticMeshComponent.h"

namespace
{
	bool DrawAxisControl(const FString& _label, float& _value, float _speed, float _minValue, float _maxValue, float _resetValue, FVector4 _color)
	{
		bool isValueChanged = false;

		float lineHeight = GImGui->Font->LegacySize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec4 color(_color.x, _color.y, _color.z, _color.w);
		ImVec2 buttonSize = { lineHeight + 2.0f , lineHeight };
		// 각 버튼의 색상 설정
		ImGui::PushStyleColor(ImGuiCol_Button, color);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ color.x + 0.1f, color.y + 0.1f, color.z + 0.1f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ color.x - 0.1f, color.y - 0.1f, color.z - 0.1f, 1.0f });

		// 굵은 폰트 적용
		if (ImGui::Button(_label.c_str(), buttonSize))
		{
			_value = _resetValue;
			isValueChanged = true;
		}
		ImGui::PopStyleColor(3);

		// 드래그 슬라이더
		ImGui::SameLine();
		FString dragID = _label;
		dragID.InsertAt(0, FString("##"));
		isValueChanged |= ImGui::DragFloat(dragID.c_str(), &_value, _speed, _minValue, _maxValue, "%.2f");

		return isValueChanged;

	}

	bool DrawVector3Controller(const FString& _label, FVector& _values, float _resetValue, float _columnWidth)
	{
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

		bool isVectorChanged = false;

		ImGui::PushID(_label.c_str());
		if (ImGui::BeginTable(_label.c_str(), 2, flags)) // 고유 ID, 열 2개, 플래그
		{
			// ImGuiTableColumnFlags_WidthFixed: 초기 너비 고정
			// ImGuiTableColumnFlags_WidthStretch: 창 크기에 따라 너비 조절 (기본값)
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, _columnWidth);
			ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(_label.c_str()); // 왼쪽 열: 레이블

			ImGui::TableSetColumnIndex(1);
			// [컨트롤러 UI 코드] // 오른쪽 열: 컨트롤러
			ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 2,0 });
			ImGui::PushFont(boldFont);

			isVectorChanged |= DrawAxisControl("X", _values.x, 0.1f, 0.0f, 0.0f, 0.0f, { 0.8f, 0.1f,0.1f, 1.0f });

			ImGui::PopItemWidth();

			ImGui::SameLine();
			isVectorChanged |= DrawAxisControl("Y", _values.y, 0.1f, 0.0f, 0.0f, 0.0f, { 0.1f, 0.8f,0.1f, 1.0f });

			ImGui::PopItemWidth();

			ImGui::SameLine();
			isVectorChanged |= DrawAxisControl("Z", _values.z, 0.1f, 0.0f, 0.0f, 0.0f, { 0.1f, 0.1f,0.8f, 1.0f });

			ImGui::PopItemWidth();

			ImGui::PopFont();
			ImGui::PopStyleVar();
			ImGui::EndTable();
		}

		ImGui::Columns(1);

		ImGui::PopID();

		return isVectorChanged;
	}
	// UClass에 등록된 프로퍼티를 타입에 맞는 위젯으로 그린다
	void DrawProperty(UObject* Object, const FProperty& Property, ImFont* CustomFont)
	{
		void* ValuePtr = reinterpret_cast<char*>(Object) + Property.Offset;
		FString Label = Property.Name;
		Label.InsertAt(0, FString("##")); // 내부 ID용 식별자 생성

		// Vector 타입은 DrawVector3Controller 내부에서 레이블을 자체적으로 그리므로 예외 처리
		if (Property.Type != EPropertyType::Vector)
		{
			ImGui::Text(Property.Name.c_str());
			ImGui::SameLine(120.0f);
			ImGui::SetNextItemWidth(-1.0f);
		}

		switch (Property.Type)
		{
		case EPropertyType::Float:
			ImGui::DragFloat(Label.c_str(), static_cast<float*>(ValuePtr), 0.1f);
			break;

		case EPropertyType::Int:
			ImGui::DragInt(Label.c_str(), static_cast<int*>(ValuePtr), 1.0f);
			break;

		case EPropertyType::Bool:
			ImGui::Checkbox(Label.c_str(), static_cast<bool*>(ValuePtr));
			break;

		case EPropertyType::Vector:
		{
			FVector* Value = static_cast<FVector*>(ValuePtr);
			DrawVector3Controller(Property.Name, *Value, 0.0f, 120.0f);
			break;
		}

		case EPropertyType::Vector4:
		{
			FVector4* Value = static_cast<FVector4*>(ValuePtr);
			ImGui::ColorEdit4(Label.c_str(), &Value->x);
			break;
		}

		case EPropertyType::String:
		{
			FString* Value = static_cast<FString*>(ValuePtr);

			char Buffer[256] = {};
			strncpy_s(Buffer, Value->c_str(), sizeof(Buffer) - 1);
			if (CustomFont) ImGui::PushFont(CustomFont);
			if (ImGui::InputText(Label.c_str(), Buffer, sizeof(Buffer)))
			{
				*Value = (FString)Buffer;
			}
			if (CustomFont) ImGui::PopFont();
			break;
		}

		case EPropertyType::WString:
		{
			std::wstring* Value = static_cast<std::wstring*>(ValuePtr);

			char Buffer[256] = {};
			size_t convertedChars = 0;
			wcstombs_s(&convertedChars, Buffer, sizeof(Buffer), Value->c_str(), _TRUNCATE);

			if (CustomFont) ImGui::PushFont(CustomFont);

			if (ImGui::InputText(Label.c_str(), Buffer, sizeof(Buffer)))
			{
				wchar_t wBuffer[256] = {};
				mbstowcs_s(&convertedChars, wBuffer, sizeof(wBuffer), Buffer, _TRUNCATE);
				*Value = std::wstring(wBuffer);
			}

			if (CustomFont) ImGui::PopFont();
			break;
		}

		case EPropertyType::Asset:
		{
			// 이 프로퍼티가 어떤 에셋 클래스를 가리키는지는 등록 시점에 FProperty에 담아 뒀다.
			if (!Property.ClassInfo)
			{
				ImGui::TextDisabled("(Unknown asset class)");
				break;
			}

			// 슬롯은 실제로 UStaticMesh* 같은 파생 포인터지만,
			// 단일 상속이라 UAsset* 과 표현이 같아 이렇게 읽고 쓴다.
			UAsset** AssetSlot = static_cast<UAsset**>(ValuePtr);
			UAsset* CurrentAsset = *AssetSlot;

			// FName::ToString()이 임시 객체를 돌려주므로 반드시 붙잡아 둔다.
			FString CurrentName = CurrentAsset ? CurrentAsset->GetAssetName().ToString() : FString("None");

			if (ImGui::BeginCombo(Label.c_str(), CurrentName.c_str()))
			{
				if (ImGui::Selectable("None", CurrentAsset == nullptr))
				{
					*AssetSlot = nullptr;
				}

				// 해당 클래스와 그 파생만 나열된다.
				for (TObjectIterator<UAsset> It(Property.ClassInfo); It; ++It)
				{
					UAsset* Asset = *It;
					const bool bSelected = (Asset == CurrentAsset);

					FString AssetName = Asset->GetAssetName().ToString();
					if (ImGui::Selectable(AssetName.c_str(), bSelected))
					{
						*AssetSlot = Asset;
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			break;
		}
		case EPropertyType::Array:
		{
			if (Property.ElementType != EPropertyType::Asset ||
				Property.ElementClassInfo == nullptr)
			{
				ImGui::TextDisabled("(Unsupported array type)");
				break;
			}

			// 현재 우리가 지원하는 배열은 TArray<UMaterial*>.
			TArray<UMaterial*>* Materials =
				static_cast<TArray<UMaterial*>*>(ValuePtr);

			for (uint32 Index = 0; Index < Materials->Num(); ++Index)
			{
				ImGui::PushID(static_cast<int>(Index));

				ImGui::Text("Slot %u", Index);
				ImGui::SameLine(120.0f);
				ImGui::SetNextItemWidth(-1.0f);

				UMaterial* CurrentMaterial = (*Materials)[Index];

				FString CurrentName =CurrentMaterial? CurrentMaterial->GetAssetName().ToString(): FString("None");

				if (ImGui::BeginCombo("##Material",CurrentName.c_str()))
				{
					// None
					if (ImGui::Selectable(
						"None",
						CurrentMaterial == nullptr))
					{
						(*Materials)[Index] = nullptr;
					}

					// Material Asset 목록
					for (TObjectIterator<UAsset> It(Property.ElementClassInfo); It; ++It)
					{
						UAsset* Asset = *It;

						if (!Asset)
						{
							continue;
						}

						UMaterial* Material = Asset->Cast<UMaterial>();

						if (!Material)
						{
							continue;
						}

						const bool bSelected =
							(Material == CurrentMaterial);

						FString AssetName =
							Material->GetAssetName().ToString();

						if (ImGui::Selectable(
							AssetName.c_str(),
							bSelected))
						{
							(*Materials)[Index] = Material;
						}

						if (bSelected)
						{
							ImGui::SetItemDefaultFocus();
						}
					}

					ImGui::EndCombo();
				}

				ImGui::PopID();
			}

			if (ImGui::Button("Add Material Slot"))
			{
				Materials->Add(nullptr);
			}

			break;
		}
		default:
			ImGui::TextDisabled("(Unsupported)");
			break;
		}
	}

	// 클래스 계층을 따라 올라가며 각 단계의 프로퍼티를 표시
	void DrawProperties(UObject* Object, ImFont* CustomFont)
	{
		if (!Object)
		{
			return;
		}

		TArray<const FClassInfo*> ClassChain;
		for (const FClassInfo* Class = Object->GetRuntimeClass(); Class; Class = Class->SuperClass)
		{
			ClassChain.Add(Class);
		}

		// 기반 클래스부터 표시
		for (auto It = ClassChain.rbegin(); It != ClassChain.rend(); ++It)
		{
			const FClassInfo* Class = *It;
			if (Class->GetProperties().IsEmpty())
			{
				continue;
			}

			ImGui::PushID(Class->Name.c_str());
			if (ImGui::CollapsingHeader(Class->Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (const FProperty& Property : Class->GetProperties())
				{
					DrawProperty(Object, Property, CustomFont);
				}
			}
			ImGui::PopID();
		}
	}
	void DrawStaticMeshMaterials(UStaticMeshComponent* Component) {
		if (!Component) {
			return;
		}

		UStaticMesh* StaticMesh = Component->GetStaticMesh();

		if (!StaticMesh) {
			return;
		}

		const TArray<FStaticMeshSection>& Sections = StaticMesh->GetSections();

		if (Sections.IsEmpty()) {
			return;
		}

		ImGui::Separator();

		if (!ImGui::CollapsingHeader("Static Mesh Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		for (uint32 SectionIndex = 0;SectionIndex < Sections.Num();++SectionIndex) {
			const FStaticMeshSection& Section = Sections[SectionIndex];

			ImGui::PushID(static_cast<int>(SectionIndex));

			ImGui::Text("Section %u", SectionIndex);

			ImGui::Text("Start Index : %u", Section.StartIndex);

			ImGui::Text("Index Count : %u", Section.IndexCount);

			ImGui::Text("Material Slot : %u", Section.MaterialSlotIndex);

			UMaterial* CurrentMaterial = Component->GetMaterial(Section.MaterialSlotIndex);

			FString CurrentMaterialName = CurrentMaterial ? CurrentMaterial->GetAssetName().ToString() : FString("None");

			ImGui::Text("Material");
			ImGui::SameLine(120.0f);
			ImGui::SetNextItemWidth(-1.0f);

			if (ImGui::BeginCombo("##Material", CurrentMaterialName.c_str())) {
				const bool bNoneSelected = (CurrentMaterial == nullptr);
				if (ImGui::Selectable("None", bNoneSelected)) {
					Component->SetMaterial(SectionIndex, nullptr);
				}

				if (bNoneSelected) {
					ImGui::SetItemDefaultFocus();
				}

				for (TObjectIterator<UAsset> It(UMaterial::GetClass());It;++It) {
					UAsset* Asset = *It;

					if (!Asset) {
						continue;
					}

					UMaterial* Material = Asset->Cast<UMaterial>();

					if (!Material) {
						continue;
					}

					const bool bSelected = (Material == CurrentMaterial);

					FString MaterialName = Material->GetAssetName().ToString();

					if (ImGui::Selectable(MaterialName.c_str(), bSelected)) {
						Component->SetMaterial(SectionIndex, Material);
					}

					if (bSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (CurrentMaterial)
			{
				ImGui::Spacing();

				// Get 함수를 사용해 현재 UV Speed 값을 가져옴
				FVector2 CurrentSpeed = CurrentMaterial->GetUVSpeed();
				float Speed[2] = { static_cast<float>(CurrentSpeed.X), static_cast<float>(CurrentSpeed.Y) };

				// ImGui에서 드래그로 값 수정 시 Set 함수 호출
				if (ImGui::DragFloat2("UV Speed", Speed, 0.00001f))
				{
					CurrentMaterial->SetUVSpeed(FVector2(Speed[0], Speed[1]));
				}
			}

			ImGui::Separator();
			ImGui::PopID();
		}
	}
}


bool FPropertyPanel::Init()
{
	ImGuiIO& io = ImGui::GetIO();

	io.Fonts->AddFontDefault();

	const char* fontPath = "Assets/Fonts/BMKkubulimTTF.ttf";
	float fontSize = 15.0f;
	const ImWchar* koreanRanges = io.Fonts->GetGlyphRangesKorean();

	//assert는 게임 빌드(나중에 추가되면) 작동 안하므로, 기본 폰트 설정으로 변경
	CustomFont = io.Fonts->AddFontFromFileTTF(fontPath, fontSize, nullptr, koreanRanges);
	if (!CustomFont)
	{
		UE_LOG_ERROR("Failed to load font: %s", fontPath);
		CustomFont = io.Fonts->AddFontDefault();
		return false;
	}

	return true;
}

void FPropertyPanel::Tick(float DeltaTime)
{
}

void FPropertyPanel::OnRender()
{
	if (Target)
	{
		FTransform Transform = Target->GetTransform();
		DrawVector3Controller("Location", Transform.Location, 0.0f, 10.0f);

		FVector Rotation = FVector(Transform.Rotation.Roll, Transform.Rotation.Pitch, Transform.Rotation.Yaw);
		DrawVector3Controller("Rotation", Rotation, 0.0f, 10.0f);

		Transform.Rotation = FRotator(Rotation.y, Rotation.z, Rotation.x);
		DrawVector3Controller("Scale", Transform.Scale, 0.0f, 10.0f);

		Target->SetLocation(Transform.Location);
		Target->SetRotation(Transform.Rotation);
		Target->SetScale(Transform.Scale);

		if (Target)
		{
			DrawProperties(Target, CustomFont);

			const TArray<UActorComponent*>& Components = Target->GetComponents();
			for (UActorComponent* Component : Components)
			{
				DrawProperties(Component, CustomFont);

				UStaticMeshComponent* StaticMeshComponent = Component->Cast<UStaticMeshComponent>();

				if (StaticMeshComponent)
				{
					DrawStaticMeshMaterials(StaticMeshComponent);
				}
			}
		}
	}
}
