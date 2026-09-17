#include "NativeFileDialog.h"

#include <Windows.h>
#include <commdlg.h>

namespace
{
    constexpr DWORD kPathBufferSize = 32768;

    OPENFILENAMEW MakeSceneDialog( HWND ownerWindow, wchar_t* pathBuffer, const std::filesystem::path& initialDirectory)
    {
        static constexpr wchar_t kSceneFilter[] =
            L"Scene Files (*.Scene)\0*.Scene\0"
            L"All Files (*.*)\0*.*\0";

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = ownerWindow;
        dialog.lpstrFile = pathBuffer;
        dialog.nMaxFile = kPathBufferSize;
        dialog.lpstrFilter = kSceneFilter;
        dialog.nFilterIndex = 1;
        dialog.lpstrInitialDir = initialDirectory.c_str();
        dialog.lpstrDefExt = L"Scene";

        return dialog;
    }
}

std::optional<std::filesystem::path> FNativeFileDialog::OpenScene(void* ownerWindow, const std::filesystem::path& initialDirectory)
{
    wchar_t pathBuffer[kPathBufferSize]{};

    OPENFILENAMEW dialog = MakeSceneDialog(
        static_cast<HWND>(ownerWindow),
        pathBuffer,
        initialDirectory);

    dialog.Flags =
        OFN_EXPLORER |
        OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST |
        OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&dialog))
    {
        // 취소 버튼도 여기로 들어오므로 오류로 취급하지 않는다.
        return std::nullopt;
    }

    return std::filesystem::path(pathBuffer);
}

std::optional<std::filesystem::path> FNativeFileDialog::SaveScene(void* ownerWindow, const std::filesystem::path& initialDirectory)
{
    wchar_t pathBuffer[kPathBufferSize]{};

    OPENFILENAMEW dialog = MakeSceneDialog(
        static_cast<HWND>(ownerWindow),
        pathBuffer,
        initialDirectory);

    dialog.Flags =
        OFN_EXPLORER |
        OFN_PATHMUSTEXIST |
        OFN_OVERWRITEPROMPT |
        OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&dialog))
    {
        return std::nullopt;
    }

    std::filesystem::path selectedPath(pathBuffer);

    if (selectedPath.extension().empty())
    {
        selectedPath.replace_extension(L".Scene");
    }

    return selectedPath;
}