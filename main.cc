#include "windows/togo.h"

int main() {
    std::wstring drive, wimPath;
    
    std::wcout << L"Windows To Go USB Creator" << std::endl;
    std::wcout << L"=========================" << std::endl;
    std::wcout << L"Warning: This program will attempt to fix, and or erase data" << std::endl;
    std::wcout << std::endl;
    
    // We need to list out the drivers that are plugged into the machine 
    std::wcout << L"Enter USB drive's Letter, Or ";
    std::wcout << L"Press Enter to fix operating system";
    std::wcin >> drive;
    
    std::wcout << L"Enter path to Windows Operating System Letter:";
    std::wcout << L"Press Enter to fix usb drive";
    std::wcin >> wimPath;

    if (drive.empty() && wimPath.empty()) {
        std::wcout << L"Both cannot be empty!\n ....Exiting program.";
        return 1;
    }
    
    // Verify inputs
    if (drive.length() < 2 || drive[1] != L':') {
        std::wcerr << L"Invalid drive format. Use format like E:" << std::endl;
        return 1;
    }

    std::wcout << L"Starting creation process..." << std::endl;
    std::wcout << L"This may take 15-30 minutes depending on USB speed." << std::endl;

    if (wimPath.empty()) {
        try {
            WindowsToGoCreator creator(drive);
            creator.OptimizeWindows();
            creator.ValidateWindows();
        }
        catch (...) {
            WindowsToGoCreator::Menu();
        }
    }
    else {

        try {
            WindowsToGoCreator creator(drive, wimPath);
            creator.OptimizeWindows();
            creator.ValidateWindows();
        }
        catch (...) {
            WindowsToGoCreator::Menu();
        }
    }
    
    return 0;
}
