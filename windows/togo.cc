#include "../windows/togo.h"

std::wstring MESSAGE, MESSAGE_ERROR;
bool WindowsToGoCreator::needsToBeWiped = false;
bool WindowsToGoCreator::existingInstallation = false;
std::wstring WindowsToGoCreator::windows = L"";
std::wstring WindowsToGoCreator::usb_drive = L"";


bool WindowsToGoCreator::ConfirmUserIntent() {

    return false;
}

void WindowsToGoCreator::ShowProgress() {

    std::wcout << MESSAGE << '\n';
}

void WindowsToGoCreator::ShowError() {

    std::wcout << MESSAGE_ERROR << '\n';
}

void WindowsToGoCreator::OptimizeWindows() {


}

void WindowsToGoCreator::ValidateWindows() {

    // We need to check all the file premissions and make sure they match and are not corrupted 
    // If corrupted we fix it 
}


wchar_t WindowsToGoCreator::GetNextAvailableDriveLetter() {
    // Find next available drive letter for ESP partition
    for (wchar_t drive = L'G'; drive <= L'Z'; drive++) {
        std::wstring drivePath = std::wstring(1, drive) + L":\\";
        UINT type = GetDriveTypeW(drivePath.c_str());
        if (type == DRIVE_NO_ROOT_DIR) {
            return drive;
        }
    }
    return L'S'; // Fallback
}

std::string WindowsToGoCreator::WideToAnsi(const std::wstring& wide) {
    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, NULL, 0, NULL, NULL);
    std::string ansi(bufferSize, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &ansi[0], bufferSize, NULL, NULL);
    return ansi;
}