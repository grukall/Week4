#include "imgui_internal.h"
#include "PropertyPanel.h"
#include "core.h"
#include "Transform.h"

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
			// 만들어두신 고급 컨트롤러를 사용하도록 변경! (레이블 폭은 120.0f로 맞춤)
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

		// --- 새로 추가된 타입들 ---

		case EPropertyType::WString:
		{
			std::wstring* Value = static_cast<std::wstring*>(ValuePtr);

			// 1. wstring -> char (ImGui 표시용 변환)
			char Buffer[256] = {};
			size_t convertedChars = 0;
			wcstombs_s(&convertedChars, Buffer, sizeof(Buffer), Value->c_str(), _TRUNCATE);

			if (CustomFont) ImGui::PushFont(CustomFont);

			// 2. ImGui 입력 처리
			if (ImGui::InputText(Label.c_str(), Buffer, sizeof(Buffer)))
			{
				// 3. char -> wstring (실제 데이터 갱신용 변환)
				wchar_t wBuffer[256] = {};
				mbstowcs_s(&convertedChars, wBuffer, sizeof(wBuffer), Buffer, _TRUNCATE);
				*Value = std::wstring(wBuffer);
			}

			if (CustomFont) ImGui::PopFont();
			break;
		}

		case EPropertyType::Asset:
		{
			// TSharedPtr 구조이므로 단순 값 복사가 불가능함.
			// 실제 구현 시에는 Content Browser나 Asset Manager와 연동하여 
			// 드래그 앤 드롭이나 콤보 박스(선택창)로 구현해야 합니다.

			// 당장은 UI 레이아웃 유지를 위해 자리표시자(Placeholder) 버튼 생성
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button(std::string("Select Asset" + (std::string)Label).c_str(), ImVec2(-1.0f, 0.0f)))
			{
				// TODO: 에셋 브라우저 팝업 열기 로직
			}
			ImGui::PopStyleColor();
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
}


bool FPropertyPanel::Init()
{
	ImGuiIO& io = ImGui::GetIO();

	io.Fonts->AddFontDefault();

	const char* fontPath = "HMKMRHD.ttf";
	float fontSize = 15.0f;
	const ImWchar* koreanRanges = io.Fonts->GetGlyphRangesKorean();

	CustomFont = io.Fonts->AddFontFromFileTTF(fontPath, fontSize, nullptr, koreanRanges);
	assert(CustomFont != nullptr);

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
			}
		}
	}
}
