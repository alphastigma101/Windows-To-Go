#ifndef _TO_GO_H_
#define _TO_GO_H_
#include <../editor/editor.h>
#include <virtdisk.h>

//#pragma comment(lib, "setupapi.lib")
//#pragma comment(lib, "VirtDisk.lib")

class WindowsToGoCreator : public Interface::CRTP<WindowsToGoCreator> {
    // This is the cli ui and handles the usb formatting
          
    public:

        explicit WindowsToGoCreator(const std::wstring& drive) {
            // Pull in the commands from BCD that validates, repairs if needed, and modifies the bcd 
            // let this class windowstogocreator to validate it
            
            usb_drive = drive;
            if (ValidateUSB()) {
                Editor::BCD bcd(usb_drive);
            }
        }

        explicit WindowsToGoCreator(const std::wstring& drive, const std::wstring& windows_drive) {

            usb_drive = drive;
            windows = windows_drive;
            // Validate the bcd and if it is corrupted, we will repair it 
            ShowProgress();
            
            // Join the thread
            try {
                // Every method here will be throw exceptions when needed
                // We need to create threads for each method called show_progress
                ShowProgress();
                ValidateUSB(); // Check the health of the usb
                // join the thread here 
                ShowProgress();
                PrepareUSB();
                // Join the thread here
                ShowProgress();
                Editor::BCD bcd(drive, windows);
                
            }
            catch(...) {

                ShowError();
                throw "Jumping to Menu!\n";
            }

          
             
        }

        ~WindowsToGoCreator() = default;
        
        static void Menu() {
            // This should be the recovery shell.
            // We should be able to run and do diagnostics and connect the status codes with our own rolled methods here

        }
        static void OptimizeWindows();
        static void ValidateWindows();
        static bool ConfirmUserIntent();
 
    private:

        static std::wstring usb_drive;
        static std::wstring windows;
        static bool existingInstallation; 
        static bool needsToBeWiped;

        static bool ValidateUSB() {

            MESSAGE = L"[VALIDATION] Validating USB drive: " + usb_drive + L"\n";

            // Step 1: Basic drive validation
            if (!ValidateDriveExistence()) {

                MESSAGE_ERROR = L"[ERROR] Drive " + usb_drive + L" does not exist or is not accessible\n";
                throw;
            }

            if (!IsRemovableUSBDrive()) {

                MESSAGE = L"[WARNING] Drive " + usb_drive + L" may not be a removable USB drive \n";

                if (!ConfirmProceedWithNonUSB()) {

                    MESSAGE_ERROR = L"[CANCELLED] User cancelled the operation\n";
                    throw;

                }
            }

            if (!CheckDriveHealth(usb_drive)) {
                MESSAGE = L"[WARNING] USB drive health check failed \n";
                if (!ConfirmProceedWithUnhealthyDrive()) throw;
            }

            existingInstallation = CheckForExistingWindows(usb_drive);
            if (existingInstallation)
                MESSAGE = L"[INFO] Existing Windows installation detected on USB drive \n";

            if (!windows.empty()) {
                // TODO: This chunk of code could be placed in a method called CheckSpace()
                ULONGLONG availableSpace = GetAvailableSpace(usb_drive);
                ULONGLONG requiredSpace = CalculateRequiredSpace();

                MESSAGE = L"[INFO] Available space: " + FormatBytes(availableSpace) + L"\n";
                MESSAGE = L"[INFO] Required space: " + FormatBytes(requiredSpace) + L"\n";

                if (availableSpace < requiredSpace) {
                    MESSAGE_ERROR = L"[ERROR] Insufficient space on USB drive \n" +
                        static_cast<std::wstring>(L"Need additional: ") + FormatBytes(requiredSpace - availableSpace) + L"\n";
                    throw;
                }

                needsToBeWiped = DetermineWipeNecessity(existingInstallation, availableSpace, requiredSpace);


                if (!GetFinalUserConfirmation(usb_drive, needsToBeWiped, existingInstallation)) {

                    MESSAGE_ERROR = L"[CANCELLED] User cancelled the operation\n";
                    throw;
                }
            }

            MESSAGE = L"[SUCCESS] USB validation completed successfully\n";
            return true;
        }

        static bool ValidateDriveExistence() {

            std::wstring rootPath = usb_drive + L"\\";
            DWORD driveType = GetDriveTypeW(rootPath.c_str());

            if (driveType == DRIVE_NO_ROOT_DIR) {

                MESSAGE_ERROR = L"[ERROR] Drive " + usb_drive + L" does not exist\n";
                throw;

            }

            if (driveType == 0) {

                MESSAGE_ERROR = L"[ERROR] Cannot determine drive type for " + usb_drive + L"\n";
                throw;
            }

            // Test write access
            std::wstring testFile = usb_drive + L"\\write_test.tmp";
            HANDLE hFile = CreateFileW(testFile.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                FILE_ATTRIBUTE_TEMPORARY, NULL);

            if (hFile == INVALID_HANDLE_VALUE) {

                MESSAGE_ERROR = L"[ERROR] No write access to drive " + usb_drive + L"\n";
                throw;

            }

            CloseHandle(hFile);
            DeleteFileW(testFile.c_str());

            MESSAGE = L"[SUCCESS] Drive " + usb_drive + L" exists and is accessible" + L"\n";
            return true;

        }

        static bool IsRemovableUSBDrive() {

            std::wstring rootPath = usb_drive + L"\\";
            UINT driveType = GetDriveTypeW(rootPath.c_str());

            if (driveType == DRIVE_REMOVABLE) {

                MESSAGE = L"[INFO] Drive " + usb_drive + L" is a removable USB drive" + L"\n";
                return true;

            }

            // Additional check using SetupAPI to verify USB device
            if (IsUSBDeviceBySetupAPI(usb_drive)) {

                MESSAGE = L"[INFO] Drive " + usb_drive +  L" is a USB device (confirmed via SetupAPI)" + L"\n";
                return true;

            }

            MESSAGE = L"[WARNING] Drive " + usb_drive + L" is not detected as removable (Type: " 
                + std::to_wstring(driveType) + L")" + L"\n";

            return false;

        }

        static bool IsUSBDeviceBySetupAPI(const std::wstring& drive) {

            std::wstring physicalPath = L"\\\\.\\" + drive.substr(0, 2);
            HANDLE hDevice = CreateFileW(physicalPath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_EXISTING, 0, NULL);

            if (hDevice == INVALID_HANDLE_VALUE) {
                return false;
            }

            STORAGE_PROPERTY_QUERY query = {};
            query.PropertyId = StorageDeviceProperty;
            query.QueryType = PropertyStandardQuery;

            STORAGE_DEVICE_DESCRIPTOR deviceDesc = {};
            DWORD bytesReturned = 0;

            BOOL result = DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY,
                &query, sizeof(query),
                &deviceDesc, sizeof(deviceDesc),
                &bytesReturned, NULL);

            CloseHandle(hDevice);

            if (result && deviceDesc.BusType == BusTypeUsb) {
                return true;
            }

            return false;
        }

        static bool CheckDriveHealth(const std::wstring& drive) {
            MESSAGE = L"[HEALTH] Checking USB drive health...\n";

            std::wstring physicalPath = L"\\\\.\\" + drive.substr(0, 2);
            HANDLE hDevice = CreateFileW(physicalPath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_EXISTING, 0, NULL);

            if (hDevice == INVALID_HANDLE_VALUE) {

                MESSAGE = L"[WARNING] Cannot open device for health check\n";
                return true; // Assume healthy if we can't check

            }

            DISK_PERFORMANCE diskPerf = {};
            DWORD bytesReturned = 0;

            if (DeviceIoControl(hDevice, IOCTL_DISK_PERFORMANCE, NULL, 0,
                &diskPerf, sizeof(diskPerf), &bytesReturned, NULL)) {

                MESSAGE = L"[INFO] Drive performance data collected\n";
            }

            // Check for write protection
            DWORD dwBytesReturned;
            BOOL isWriteProtected = FALSE;
            if (DeviceIoControl(hDevice, IOCTL_DISK_IS_WRITABLE, NULL, 0, NULL, 0, &dwBytesReturned, NULL)) 
                MESSAGE = L"[INFO] Drive is writable\n";
            else {

                MESSAGE_ERROR = L"[ERROR] Drive is write-protected or read-only\n";
                CloseHandle(hDevice);
                throw;
            }

            CloseHandle(hDevice);

            // Perform file system check
            if (!CheckFileSystemHealth(drive)) {

                MESSAGE = L"[WARNING] File system issues detected\n";
                return false;
            }

            MESSAGE = L"[SUCCESS] USB drive health check passed\n";
            return true;
        }

        static bool CheckFileSystemHealth(const std::wstring& drive) {

            std::wstring rootPath = drive + L"\\";

            // Check for common file system issues
            DWORD sectorsPerCluster, bytesPerSector, freeClusters, totalClusters;
            if (GetDiskFreeSpaceW(rootPath.c_str(), &sectorsPerCluster, &bytesPerSector,
                &freeClusters, &totalClusters)) {

                MESSAGE = L"[INFO] File system: " + std::to_wstring(bytesPerSector * sectorsPerCluster) 
                    + L" bytes per cluster\n";

                MESSAGE = L"[INFO] Total clusters: " + std::to_wstring(totalClusters) 
                    + L", Free clusters: " + std::to_wstring(freeClusters) + L"\n";

                return true;
            }

            return false;
        }

        // TODO: This will need to be modified that it supports both the usb and windows
        static bool CheckForExistingWindows(const std::wstring& drive) {

            static std::vector<std::wstring> windowsIndicators = {
                drive + L"\\Windows\\System32",
                drive + L"\\Windows\\System32\\winload.exe",
                drive + L"\\Windows\\System32\\ntoskrnl.exe",
                drive + L"\\Boot\\BCD",
                drive + L"\\Windows\\System32\\config\\SYSTEM"
            };

            int foundIndicators = 0;
            for (const auto& indicator : windowsIndicators) {
                if (PathExists(indicator)) {
                    foundIndicators++;
                    MESSAGE = L"[DETECTED] " + indicator + L"\n";
                }
            }

            // If we found multiple Windows indicators, assume it's a Windows installation
            return (foundIndicators >= 3);
        }

        static ULONGLONG GetAvailableSpace(const std::wstring& drive) {

            std::wstring rootPath = drive + L"\\";
            ULARGE_INTEGER freeBytes, totalBytes, totalFreeBytes;

            if (GetDiskFreeSpaceExW(rootPath.c_str(), &freeBytes, &totalBytes, &totalFreeBytes)) {
                return freeBytes.QuadPart;
            }

            return 0;
        }

        static ULONGLONG CalculateRequiredSpace() {
            // Estimate required space for Windows installation
            // These are conservative estimates for different Windows versions

            ULONGLONG baseSpace = 10ULL * 1024 * 1024 * 1024; // 10 GB base
            ULONGLONG systemSpace = 5ULL * 1024 * 1024 * 1024; // 5 GB for system files
            ULONGLONG bufferSpace = 2ULL * 1024 * 1024 * 1024; // 2 GB buffer

            return baseSpace + systemSpace + bufferSpace; // ~17 GB total
        }

        static bool DetermineWipeNecessity(bool hasExistingWindows, ULONGLONG availableSpace, ULONGLONG requiredSpace) {
            if (hasExistingWindows) {

                MESSAGE = L"\n[ACTION REQUIRED] Existing Windows installation detected!\n";
                MESSAGE = L"Options:\n";
                MESSAGE = L"1. Wipe drive and create fresh Windows To Go\n";
                MESSAGE = L"2. Modify existing installation (advanced)\n";

                return GetUserWipePreference();
            }

            // If no existing Windows but low space, might need to wipe other data
            if (availableSpace < requiredSpace * 1.2) { // 20% buffer
                MESSAGE = L"\n[WARNING] Low available space for optimal performance\n";
                MESSAGE = L"Consider wiping drive to free up space\n";
                return GetUserWipePreference();
            }

            return false; // No wipe needed by default
        }

        static bool GetUserWipePreference() {

            MESSAGE = L"Do you want to wipe the USB drive ? (yes / no) : ";
            std::wstring response;
            std::wcin >> response;

            return (response == L"yes" || response == L"y" || response == L"1");
        }

        static bool ConfirmProceedWithNonUSB() {

            MESSAGE = L"\n[WARNING] This may not be a removable USB drive.\n";
            MESSAGE = L"Continuing may risk data loss on fixed drives.\n";
            MESSAGE = L"Do you want to proceed? (yes/no): ";

            std::wstring response;
            std::wcin >> response;

            return (response == L"yes" || response == L"y");
        }

        static bool ConfirmProceedWithUnhealthyDrive() {

            MESSAGE = L"\n[WARNING] Drive health check failed.\n";
            MESSAGE = L"Windows To Go may not work reliably on this drive.\n";
            MESSAGE = L"Do you want to proceed anyway? (yes/no): ";

            std::wstring response;
            std::wcin >> response;

            return (response == L"yes" || response == L"y");
        }

        static bool GetFinalUserConfirmation(const std::wstring& drive, bool needsWipe, bool hasExistingWindows) {
            MESSAGE = L"\n=== FINAL CONFIRMATION ===\n";
            MESSAGE = L"USB Drive: " + drive + L"\n";
            MESSAGE = L"Wipe required: " + static_cast<std::wstring>((needsWipe ? L"YES" : L"NO")) + L"\n";
            MESSAGE = L"Existing Windows: " + static_cast<std::wstring>((hasExistingWindows ? L"YES" : L"NO")) + L"\n";

            if (needsWipe) 
                MESSAGE = L"WARNING: All data on " + drive + L" will be permanently deleted!\n";

            MESSAGE = L"\nProceed with Windows To Go creation? (yes/no): ";

            std::wstring response;
            std::wcin >> response;

            return (response == L"yes" || response == L"y");
        }

        // Utility functions
        static bool PathExists(const std::wstring& path) {
            DWORD attributes = GetFileAttributesW(path.c_str());
            return (attributes != INVALID_FILE_ATTRIBUTES);
        }

        static std::wstring FormatBytes(ULONGLONG bytes) {
            const wchar_t* sizes[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
            int order = 0;
            double size = static_cast<double>(bytes);

            while (size >= 1024 && order < 4) {
                order++;
                size /= 1024;
            }

            std::wstringstream ss;
            ss.precision(2);
            ss << std::fixed << size << L" " << sizes[order];
            return ss.str();
        }

        // Used only if usb_drive and windows variable is not empty
        static bool PrepareUSB() {

            MESSAGE = L"[PREPARE] Preparing USB drive: " + usb_drive + L"\n";
            MESSAGE = L"[PREPARE] Source Windows: " + windows + L"\n";

            int diskNumber = GetPhysicalDiskNumber(usb_drive);
            if (diskNumber == -1) {

                MESSAGE_ERROR = L"[ERROR] Cannot get physical disk number for " + usb_drive + L"\n";
                throw;
            }

            MESSAGE = L"[INFO] Physical disk number: " + std::to_wstring(diskNumber) + L"\n";

            
            if (!CleanDisk(diskNumber)) {

                MESSAGE_ERROR = L"[ERROR] Failed to clean disk\n";
                throw;
            }

        
            if (!ConvertToGPT(diskNumber)) {

                MESSAGE_ERROR = L"[ERROR] Failed to convert to GPT\n";
                throw;

            }

            
            if (!CreateUSBPartitions(diskNumber, usb_drive)) {

                MESSAGE_ERROR = L"[ERROR] Failed to create partitions\n";
                throw;
            }

            if (!FormatPartitions(usb_drive)) {

                MESSAGE_ERROR = L"[ERROR] Failed to format partitions\n";
                throw;
            }

           
            if (!CopyWindowsFiles(windows, usb_drive)) {

               MESSAGE_ERROR = L"[ERROR] Failed to copy Windows files\n";
               throw;

            }

           
            if (!InstallBootFiles(usb_drive)) {

                MESSAGE_ERROR = L"[ERROR] Failed to install boot files\n";
                throw;

            }

            MESSAGE = L"[SUCCESS] USB preparation completed successfully\n";
            return true;
        }

        static int GetPhysicalDiskNumber(const std::wstring& drive) {
            std::wstring physicalPath = L"\\\\.\\" + drive.substr(0, 2);

            HANDLE hDevice = CreateFileW(physicalPath.c_str(), GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_EXISTING, 0, NULL);

            if (hDevice == INVALID_HANDLE_VALUE) {

                MESSAGE_ERROR = L"[ERROR] Cannot open device: " + physicalPath + L"\n";
                throw;
            }

            STORAGE_DEVICE_NUMBER deviceNumber = {};
            DWORD bytesReturned = 0;

            BOOL result = DeviceIoControl(hDevice, IOCTL_STORAGE_GET_DEVICE_NUMBER,
                NULL, 0, &deviceNumber, sizeof(deviceNumber),
                &bytesReturned, NULL);

            CloseHandle(hDevice);

            if (!result) {

                MESSAGE_ERROR = L"[ERROR] Cannot get device number, error: " + std::to_wstring(GetLastError()) + L"\n";
                throw;
            }

            return deviceNumber.DeviceNumber;
        }

        static bool CleanDisk(int diskNumber) {
            MESSAGE = L"[CLEAN] Cleaning disk " + std::to_wstring(diskNumber) + L" (removing all partitions)...\n";

            std::wstring cleanScript =
                L"select disk " + std::to_wstring(diskNumber) + L"\n" +
                L"clean\n" +
                L"exit\n";

            return ExecuteDiskPartScript(cleanScript);
        }

        static bool ConvertToGPT(int diskNumber) {
            MESSAGE = L"[CONVERT] Converting disk " + std::to_wstring(diskNumber) + L" to GPT...\n";

            std::wstring convertScript =
                L"select disk " + std::to_wstring(diskNumber) + L"\n" +
                L"convert gpt\n" +
                L"exit\n";

            return ExecuteDiskPartScript(convertScript);
        }

        static bool CreateUSBPartitions(int diskNumber, const std::wstring& usbDrive) {
            MESSAGE = L"[PARTITION] Creating partitions on disk " + std::to_wstring(diskNumber) + L"...\n";

            // Create EFI System Partition (ESP) - 100MB
            // Create Windows Partition - rest of the space
            std::wstring partitionScript =
                L"select disk " + std::to_wstring(diskNumber) + L"\n" +
                L"create partition efi size=100\n" +
                L"format quick fs=fat32 label=\"System\"\n" +
                L"assign letter=" + GetNextAvailableDriveLetter() + L"\n" +
                L"create partition primary\n" +
                L"format quick fs=ntfs label=\"Windows\"\n" +
                L"assign letter=" + usbDrive.substr(0, 1) + L"\n" +
                L"active\n" +
                L"exit\n";

            return ExecuteDiskPartScript(partitionScript);
        }

        static bool ExecuteDiskPartScript(const std::wstring& script) {
            std::wstring scriptFile = L"diskpart_script.txt";

            // Write script to temporary file
            HANDLE hFile = CreateFileW(scriptFile.c_str(), GENERIC_WRITE, 0, NULL,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {

                MESSAGE_ERROR = L"[ERROR] Cannot create script file\n";
                throw;
            }

            DWORD bytesWritten;
            std::string ansiScript = WideToAnsi(script);
            WriteFile(hFile, ansiScript.c_str(), ansiScript.length(), &bytesWritten, NULL);
            CloseHandle(hFile);

            // Execute diskpart with the script
            std::wstring command = L"diskpart /s " + scriptFile;

            STARTUPINFO si = { sizeof(si) };
            PROCESS_INFORMATION pi;

            if (CreateProcessW(NULL, &command[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                WaitForSingleObject(pi.hProcess, 60000); // 60 second timeout
                DWORD exitCode;
                GetExitCodeProcess(pi.hProcess, &exitCode);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);

                // Clean up script file
                DeleteFileW(scriptFile.c_str());

                if (exitCode == 0) {

                    MESSAGE = L"[SUCCESS] DiskPart operation completed\n";
                    return true;
                }
                else {

                    MESSAGE_ERROR = L"[ERROR] DiskPart failed with exit code: " + std::to_wstring(exitCode) + L"\n";
                    throw;
                }
            }

            DeleteFileW(scriptFile.c_str());
            return false;
        }

        static bool FormatPartitions(const std::wstring& usbDrive) {
            MESSAGE = L"[FORMAT] Verifying partition formatting...\n";

            // The partitions should already be formatted by diskpart, but verify
            std::wstring windowsPath = usbDrive + L"\\";
            DWORD fileSystemFlags;
            wchar_t fileSystemName[MAX_PATH];

            if (GetVolumeInformationW(windowsPath.c_str(), NULL, 0, NULL, NULL,
                &fileSystemFlags, fileSystemName, MAX_PATH)) {
                MESSAGE = L"[INFO] Windows partition file system: " + std::wstring(fileSystemName) + L"\n";

                if (wcscmp(fileSystemName, L"NTFS") != 0) {

                    MESSAGE = L"[WARNING] Windows partition is not NTFS, reformatting...\n";
                    return FormatDrive(usbDrive, L"NTFS", L"Windows");

                }
            }

            return true;
        }

        static bool FormatDrive(const std::wstring& drive, const std::wstring& fileSystem, const std::wstring& label) {
            std::wstring formatCommand = L"format " + drive + L" /FS:" + fileSystem + L" /Q /Y";
            if (!label.empty()) {
                formatCommand += L" /V:" + label;
            }

            STARTUPINFO si = { sizeof(si) };
            PROCESS_INFORMATION pi;

            if (CreateProcessW(L"cmd.exe", &(L"/c " + formatCommand)[0], NULL, NULL, FALSE,
                CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                WaitForSingleObject(pi.hProcess, 120000); // 2 minute timeout
                DWORD exitCode;
                GetExitCodeProcess(pi.hProcess, &exitCode);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);

                return (exitCode == 0);
            }

            return false;
        }

        static bool CopyWindowsFiles(const std::wstring& sourceDrive, const std::wstring& destDrive) {
            MESSAGE = L"[COPY] Copying Windows files from " + sourceDrive + L" to " + destDrive + L"...\n";

             static std::vector<std::wstring> directoriesToCopy = {
                L"Windows",
                L"Program Files",
                L"ProgramData",
                L"Users"
            };

            static std::vector<std::wstring> filesToCopy = {
                L"bootmgr",
                L"BOOTNXT"
            };

            // Create directory structure first
            for (const auto& dir : directoriesToCopy) {
                std::wstring sourceDir = sourceDrive + L"\\" + dir;
                std::wstring destDir = destDrive + L"\\" + dir;

                if (PathExists(sourceDir)) {
                    MESSAGE = L"[COPY] Copying directory: " + dir + L"\n";
                    if (!CopyDirectoryRecursive(sourceDir, destDir)) {
                        MESSAGE = L"[WARNING] Failed to copy directory: " + dir + L"\n";
                    }
                }
            }

            // Copy individual files
            for (const auto& file : filesToCopy) {
                std::wstring sourceFile = sourceDrive + L"\\" + file;
                std::wstring destFile = destDrive + L"\\" + file;

                if (PathExists(sourceFile)) {
                    MESSAGE = L"[COPY] Copying file: " + file + L"\n";
                    if (!CopyFileW(sourceFile.c_str(), destFile.c_str(), FALSE)) {
                        MESSAGE = L"[WARNING] Failed to copy file: " + file + L"\n";
                    }
                }
            }

            // Copy Boot directory separately (critical for booting)
            std::wstring bootSource = sourceDrive + L"\\Boot";
            std::wstring bootDest = destDrive + L"\\Boot";

            if (PathExists(bootSource)) {
                MESSAGE = L"[COPY] Copying Boot directory...\n";
                if (!CopyDirectoryRecursive(bootSource, bootDest)) {

                    // I think we can recover this by building the files again
                    MESSAGE = L"[ERROR] Failed to copy Boot directory\n";
                    return false;

                }
            }

            return true;
        }

        static bool CopyDirectoryRecursive(const std::wstring& source, const std::wstring& dest) {
            // Create destination directory
            CreateDirectoryW(dest.c_str(), NULL);

            // Find first file in directory
            std::wstring searchPattern = source + L"\\*";
            WIN32_FIND_DATAW findData;
            HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);

            if (hFind == INVALID_HANDLE_VALUE) {
                return false;
            }

            do {
                // Skip . and .. directories
                if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                    continue;
                }

                std::wstring sourcePath = source + L"\\" + findData.cFileName;
                std::wstring destPath = dest + L"\\" + findData.cFileName;

                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    // Recursively copy subdirectory
                    if (!CopyDirectoryRecursive(sourcePath, destPath)) {
                        FindClose(hFind);
                        return false;
                    }
                }
                else {
                    // Copy file
                    if (!CopyFileW(sourcePath.c_str(), destPath.c_str(), FALSE)) {
                        // If copy fails due to sharing violation, try with backup semantics
                        if (GetLastError() == ERROR_SHARING_VIOLATION) {
                            if (!CopyFileWithBackup(sourcePath, destPath)) {
                                MESSAGE = L"[WARNING] Cannot copy: " + std::wstring(findData.cFileName) + L"\n";
                                // Continue with other files
                            }
                        }
                        else {
                            MESSAGE = L"[WARNING] Cannot copy: " + std::wstring(findData.cFileName) + L"\n";
                        }
                    }
                }
            } while (FindNextFileW(hFind, &findData));

            FindClose(hFind);
            return true;
        }

        static bool CopyFileWithBackup(const std::wstring& source, const std::wstring& dest) {
            // Use Volume Shadow Copy or other methods for locked files
            // For now, skip system files that are in use
            return false; // Skip this file
        }

        static bool InstallBootFiles(const std::wstring& usbDrive) {
            MESSAGE = L"[BOOT] Installing boot files...\n";

            // Use bcdboot to install boot files
            std::wstring bcdbootCommand = L"bcdboot " + usbDrive + L"\\Windows /s " + usbDrive + L" /f ALL";

            STARTUPINFO si = { sizeof(si) };
            PROCESS_INFORMATION pi;

            if (CreateProcessW(NULL, &(L"bcdboot.exe " + bcdbootCommand)[0], NULL, NULL, FALSE,
                CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                WaitForSingleObject(pi.hProcess, 30000); // 30 second timeout
                DWORD exitCode;
                GetExitCodeProcess(pi.hProcess, &exitCode);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);

                if (exitCode == 0) {

                    MESSAGE = L"[SUCCESS] Boot files installed successfully\n";
                    return true;
                }
                else {

                    MESSAGE_ERROR = L"[ERROR] bcdboot failed with exit code: " + std::to_wstring(exitCode) + L"\n";
                    throw;

                }
            }

            return false;
        }

        // Utility functions
        static wchar_t GetNextAvailableDriveLetter();
        static std::string WideToAnsi(const std::wstring& wide);

        static void ShowProgress();
        static void ShowError();

};

#endif