#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

class WindowsToGoCreator {
private:
    std::wstring usbDrive;
    std::wstring windowsImagePath;
    
public:
    WindowsToGoCreator(const std::wstring& drive, const std::wstring& imagePath)
        : usbDrive(drive), windowsImagePath(imagePath) {}

    bool CreateBootableWindowsUSB();
    
private:
    bool CleanAndPartitionUSB();
    bool ApplyWindowsImage();
    bool InstallBootManager();
    bool MakeUSBBootable();
    bool ConfigureBCD();
    int GetPhysicalDiskNumber();
};

bool WindowsToGoCreator::CreateBootableWindowsUSB() {
    std::wcout << L"Creating Windows To Go USB on drive: " << usbDrive << std::endl;
    
    // Step 1: Prepare USB drive
    if (!CleanAndPartitionUSB()) {
        std::wcerr << L"Failed to prepare USB drive" << std::endl;
        return false;
    }
    
    // Step 2: Apply Windows image
    if (!ApplyWindowsImage()) {
        std::wcerr << L"Failed to apply Windows image" << std::endl;
        return false;
    }
    
    // Step 3: Install boot manager
    if (!InstallBootManager()) {
        std::wcerr << L"Failed to install boot manager" << std::endl;
        return false;
    }
    
    // Step 4: Configure BCD
    if (!ConfigureBCD()) {
        std::wcerr << L"Failed to configure boot configuration" << std::endl;
        return false;
    }
    
    std::wcout << L"Windows To Go USB created successfully!" << std::endl;
    return true;

  #include <setupapi.h>
#include <winioctl.h>

bool WindowsToGoCreator::CleanAndPartitionUSB() {
    std::wcout << L"Cleaning and partitioning USB drive..." << std::endl;
    
    int diskNumber = GetPhysicalDiskNumber();
    if (diskNumber == -1) {
        return false;
    }
    
    // Create diskpart script
    std::wstring scriptContent = 
        L"select disk " + std::to_wstring(diskNumber) + L"\n" +
        L"clean\n" +
        L"convert gpt\n" +
        L"create partition primary size=500\n" +
        L"format quick fs=ntfs label=\"Windows To Go\"\n" +
        L"assign letter=" + usbDrive.substr(0, 1) + L"\n" +
        L"active\n" +
        L"exit\n";
    
    // Save script to temporary file
    std::wofstream scriptFile(L"partition_script.txt");
    scriptFile << scriptContent;
    scriptFile.close();
    
    // Execute diskpart
    std::wstring command = L"diskpart /s partition_script.txt";
    
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    if (CreateProcessW(NULL, &command[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        DeleteFileW(L"partition_script.txt");
        return (exitCode == 0);
    }
    
    return false;
}

int WindowsToGoCreator::GetPhysicalDiskNumber() {
    std::wstring physicalPath = L"\\\\.\\" + usbDrive.substr(0, 2);
    
    HANDLE hDrive = CreateFileW(physicalPath.c_str(), GENERIC_READ, 
                               FILE_SHARE_READ | FILE_SHARE_WRITE, 
                               NULL, OPEN_EXISTING, 0, NULL);
    
    if (hDrive == INVALID_HANDLE_VALUE) {
        return -1;
    }
    
    STORAGE_DEVICE_NUMBER deviceNumber;
    DWORD bytesReturned;
    
    if (DeviceIoControl(hDrive, IOCTL_STORAGE_GET_DEVICE_NUMBER, 
                       NULL, 0, &deviceNumber, sizeof(deviceNumber), 
                       &bytesReturned, NULL)) {
        CloseHandle(hDrive);
        return deviceNumber.DeviceNumber;
    }
    
    CloseHandle(hDrive);
    return -1;
}
  #include <wimlib.h>
#pragma comment(lib, "wimlib.lib")

bool WindowsToGoCreator::ApplyWindowsImage() {
    std::wcout << L"Applying Windows image to USB drive..." << std::endl;
    
    // Initialize wimlib
    int ret = wimlib_global_init(0);
    if (ret != 0) {
        std::wcerr << L"Failed to initialize wimlib: " << ret << std::endl;
        return false;
    }
    
    // Open WIM file
    WIMStruct* wim = nullptr;
    ret = wimlib_open_wim(windowsImagePath.c_str(), 0, &wim);
    if (ret != 0) {
        std::wcerr << L"Failed to open WIM file: " << ret << std::endl;
        return false;
    }
    
    // Apply image to USB drive
    std::wstring applyPath = usbDrive + L"\\";
    ret = wimlib_extract_image(wim, 1, applyPath.c_str(), 
                              WIMLIB_EXTRACT_FLAG_ALLOW_SPARSE);
    
    wimlib_free(wim);
    wimlib_global_cleanup();
    
    return (ret == 0);
}

  #include <bcd.h>
#pragma comment(lib, "bcd.lib")

bool WindowsToGoCreator::InstallBootManager() {
    std::wcout << L"Installing boot manager..." << std::endl;
    
    // Use bootsect to make USB bootable
    std::wstring bootSectCommand = 
        L"bootsect /nt60 " + usbDrive + L" /force /mbr";
    
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    if (CreateProcessW(NULL, &bootSectCommand[0], NULL, NULL, FALSE, 
                      CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 30000); // 30 second timeout
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        return (exitCode == 0);
    }
    
    return false;
}
}


bool WindowsToGoCreator::ConfigureBCD() {
    std::wcout << L"Configuring Boot Configuration Data..." << std::endl;
    
    // Create BCD store on USB
    std::wstring bcdEditCommands[] = {
        L"bcdedit /createstore " + usbDrive + L"\\boot\\bcd",
        L"bcdedit /store " + usbDrive + L"\\boot\\bcd /create /d \"Windows To Go\" /application osloader",
        L"bcdedit /store " + usbDrive + L"\\boot\\bcd /set {default} device partition=" + usbDrive,
        L"bcdedit /store " + usbDrive + L"\\boot\\bcd /set {default} osdevice partition=" + usbDrive,
        L"bcdedit /store " + usbDrive + L"\\boot\\bcd /set {default} path \\windows\\system32\\winload.efi",
        L"bcdedit /store " + usbDrive + L"\\boot\\bcd /set {default} systemroot \\windows",
        L"bcdedit /store " + usbDrive + L"\\boot\\bcd /set {default} detecthal yes",
        L"bcdedit /store " + usbDrive + L"\\boot\\bcd /set {default} winpe no"
    };
    
    for (const auto& command : bcdEditCommands) {
        if (!ExecuteSystemCommand(command)) {
            return false;
        }
    }
    
    return true;
}

bool ExecuteSystemCommand(const std::wstring& command) {
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    if (CreateProcessW(NULL, &command[0], NULL, NULL, FALSE, 
                      CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 15000); // 15 second timeout
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        return (exitCode == 0);
    }
    
    return false;


  // MAin application

  #include <iostream>
#include <string>

int main() {
    std::wstring drive, wimPath;
    
    std::wcout << L"Windows To Go USB Creator" << std::endl;
    std::wcout << L"=========================" << std::endl;
    std::wcout << L"Warning: This will erase all data on the target USB drive!" << std::endl;
    std::wcout << std::endl;
    
    std::wcout << L"Enter USB drive letter (e.g., E:): ";
    std::wcin >> drive;
    
    std::wcout << L"Enter path to Windows WIM/ESD file: ";
    std::wcin >> wimPath;
    
    // Verify inputs
    if (drive.length() < 2 || drive[1] != L':') {
        std::wcerr << L"Invalid drive format. Use format like E:" << std::endl;
        return 1;
    }
    
    WindowsToGoCreator creator(drive, wimPath);
    
    std::wcout << L"Starting creation process..." << std::endl;
    std::wcout << L"This may take 15-30 minutes depending on USB speed." << std::endl;
    
    if (creator.CreateBootableWindowsUSB()) {
        std::wcout << L"Success! You can now boot from this USB drive." << std::endl;
        std::wcout << L"Remember to set USB as first boot device in BIOS/UEFI." << std::endl;
    } else {
        std::wcerr << L"Failed to create Windows To Go USB." << std::endl;
        return 1;
    }
    
    return 0;
}
}
