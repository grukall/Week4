#pragma once

#include "ImGui/imgui.h"
#include "TMap.h"
#include <functional>

class ConsoleWindow final {
public:
	bool bIsOpened = true;
	bool bShowLog = true;
	bool bShowWarn = true;
	bool bShowError = true;
	char InputBuf[256];
	ImVector<const char*> Commands;
	ImVector<char*> History;
	int HistoryPos = -1;    // -1: new line, 0..History.Size-1 browsing history.
	ImGuiTextFilter Filter;
	bool AutoScroll = true;
	bool ScrollToBottom;
	bool bFocusInputRequested = false;
	ImVector<const char*> Suggestions;
	int SuggestionIndex = -1;
	int MaxLine = 256;
	static constexpr float HEIGHT_RATIO = 0.3f;

	void Process(float panelWidth);

	void RequestFocus()
	{
		bIsOpened = true;
		bFocusInputRequested = true;
		ImGui::GetIO().InputQueueCharacters.resize(0); // 이번 프레임 문자 큐를 비워 토글 키 문자가 InputText에 찍히는 것 방지
	}

	static ConsoleWindow& Get() {
		static ConsoleWindow Instance;
		return Instance;
	}
	void Init(int MaxLines) {
		if (MaxLines > MaxLine) return;
		MaxLine = MaxLines;
	};
	ConsoleWindow(const ConsoleWindow&) = delete;
	ConsoleWindow& operator=(const ConsoleWindow&) = delete;
private:
	ConsoleWindow();
	~ConsoleWindow();

	static int TextEditCallbackStub(ImGuiInputTextCallbackData* data)
	{
		ConsoleWindow* console = (ConsoleWindow*)data->UserData;
		return console->TextEditCallback(data);
	}
	int TextEditCallback(ImGuiInputTextCallbackData* data);
	void ExecCommand(const char* command_line);
	void UpdateSuggestions();
	void DrawSuggestionPopup(const ImVec2& InputMin);

	TMap<FString, std::function<void(const char*)>> CommandMap;
};
