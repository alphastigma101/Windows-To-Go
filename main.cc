#include "windows.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::wstring drive, wimPath;
    
    std::wcout << L"Windows To Go USB Creator" << std::endl;
    std::wcout << L"=========================" << std::endl;
    std::wcout << L"Warning: This will erase all data on the target USB drive!" << std::endl;
    std::wcout << std::endl;
    
    std::wcout << L"Enter USB drive's Letter:";
    std::wcin >> drive;
    
    std::wcout << L"Enter path to Windows Operating System Letter:";
    std::wcin >> wimPath;
    
    // Verify inputs
    if (drive.length() < 2 || drive[1] != L':') {
        std::wcerr << L"Invalid drive format. Use format like E:" << std::endl;
        return 1;
    }

    std::wcout << L"Starting creation process..." << std::endl;
    std::wcout << L"This may take 15-30 minutes depending on USB speed." << std::endl;
    
    WindowsToGoCreator creator(drive, wimPath);
    creator.
    
    return 0;
}
