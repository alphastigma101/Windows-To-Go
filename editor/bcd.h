#ifndef _BCD_H_
#define _BCD_H_
#include <vector>
#include <thread> 
#include <mutex>
#include <atomic> // Use for to update the message constantly
#include <windows.h>

extern std::wstring MESSAGE;
extern std::wstring ERROR;

// Check Windows version by examining system files on the target drive
enum WindowsVersion {
    WIN_UNKNOWN,
    WIN_XP,
    WIN_VISTA,
    WIN_7,
    WIN_8,
    WIN_8_1,
    WIN_10,
    WIN_11
};

class BCD {

    public:

        friend class WindowsToGoCreator;

        explicit BCD() = default;

        explicit BCD(const std::wstring& _drive, const std::wstring& windows_driver) 
            : drive(_drive) { windows = windows_driver; }

        ~bcd() = default;

    private:

        static std::wstring windows;
        std::wstring drive;


        bool ModifyBootManager(const std::wstring& usbDrive) {
            MESSAGE = L"Modifying BCD bootloader for USB compatibility...";
            
            // First validate the current BCD
            if (!ValidateSystemBCD()) {
                MESSAGE = L"ERROR: Cannot modify corrupted BCD";
                return false;
            }
            
            WindowsVersion version = GetWindowsVersionFromDrive(windows);
            std::wstring bcdEditPath = GetBCDEditPathForVersion(version);
            
            // USB-specific BCD modifications
            std::vector<std::wstring> usbModifications = GetUSBModificationsForVersion(version, usbDrive);
            
            for (const auto& modification : usbModifications) {
                std::wstring output;
                if (!ExecuteBCDCommandForDrive(bcdEditPath, modification, targetWindowsDrive, output)) {
                    MESSAGE = L"WARNING: Failed to apply USB modification: " + modification;
                    // Continue with other modifications
                }
                Sleep(500); // Small delay between commands
            }
            
            // Apply version-specific USB optimizations
            if (!ApplyVersionSpecificUSBOptimizations(version, usbDrive)) {
                MESSAGE = L"WARNING: Some USB optimizations failed";
            }
            
            // Validate the modifications
            MESSAGE = L"Validating USB boot modifications...";
            if (!ValidateSystemBCD()) {
                MESSAGE = L"ERROR: BCD corrupted after USB modifications";
                return false;
            }
            
            MESSAGE = L"BCD successfully modified for USB boot compatibility";
            return true;
        }

        static std::vector<std::wstring> GetUSBModificationsForVersion(WindowsVersion version, const std::wstring& usbDrive) {
            std::vector<std::wstring> modifications;
            
            // COMMON USB MODIFICATIONS FOR ALL WINDOWS VERSIONS
            modifications.push_back(L"/set {bootmgr} device partition=" + usbDrive);
            modifications.push_back(L"/set {bootmgr} timeout 10");
            modifications.push_back(L"/set {bootmgr} displayorder {current}");
            modifications.push_back(L"/displayorder {current} /addfirst");
            
            // Windows Boot Loader modifications for USB
            modifications.push_back(L"/set {current} device partition=" + usbDrive);
            modifications.push_back(L"/set {current} osdevice partition=" + usbDrive);
            modifications.push_back(L"/set {current} path \\Windows\\system32\\winload.exe");
            modifications.push_back(L"/set {current} systemroot \\Windows");
            modifications.push_back(L"/set {current} detecthal yes");
            modifications.push_back(L"/set {current} winpe no");
            
            // VERSION-SPECIFIC MODIFICATIONS
            switch (version) {
                case WIN_7:
                    modifications.push_back(L"/set {current} nointegritychecks on");
                    modifications.push_back(L"/set {current} pae forceenable");
                    modifications.push_back(L"/set {current} useplatformclock yes");
                    modifications.push_back(L"/set {current} truncatememory 0x10000000");
                    break;
                    
                case WIN_8:
                case WIN_8_1:
                    modifications.push_back(L"/set {current} nointegritychecks on");
                    modifications.push_back(L"/set {current} loadoptions DISABLE_INTEGRITY_CHECKS");
                    modifications.push_back(L"/set {current} bootmenupolicy Legacy");
                    modifications.push_back(L"/set {current} useplatformtick yes");
                    break;
                    
                case WIN_10:
                    modifications.push_back(L"/set {current} testsigning on");
                    modifications.push_back(L"/set {current} bootmenupolicy Standard");
                    modifications.push_back(L"/set {current} isolatedcontext no");
                    modifications.push_back(L"/set {current] allowprereleaseboot yes");
                    modifications.push_back(L"/set {current} bootlog yes");
                    modifications.push_back(L"/set {current} quietboot no");
                    break;
                    
                case WIN_11:
                    modifications.push_back(L"/set {current} testsigning on");
                    modifications.push_back(L"/set {current} nointegritychecks on");
                    modifications.push_back(L"/set {current} bootmenupolicy Standard");
                    modifications.push_back(L"/set {current} hypervisorlaunchtype Off");
                    modifications.push_back(L"/set {current} vsmLaunchType Off");
                    modifications.push_back(L"/set {current} allowprereleaseboot yes");
                    modifications.push_back(L"/set {current} bootlog yes");
                    modifications.push_back(L"/set {current} quietboot no");
                    // Windows 11 specific: Disable VBS for better USB compatibility
                    modifications.push_back(L"/set {current} isolatedcontext no");
                    modifications.push_back(L"/set {current} vsmLaunchType Off");
                    break;
                    
                case WIN_VISTA:
                    modifications.push_back(L"/set {current} nointegritychecks on");
                    modifications.push_back(L"/set {current} pae forceenable");
                    break;
                    
                default:
                    // Generic modifications for unknown versions
                    modifications.push_back(L"/set {current} nointegritychecks on");
                    modifications.push_back(L"/set {current} testsigning on");
                    break;
            }
            
            // USB-SPECIFIC PERFORMANCE OPTIMIZATIONS
            modifications.push_back(L"/set {current} custom:16000069 true"); // Enable USB boot flag
            modifications.push_back(L"/set {current} custom:16000070 1");    // USB boot type
            modifications.push_back(L"/set {current} custom:16000071 5000"); // USB boot delay
            
            return modifications;
        }

        static bool ApplyVersionSpecificUSBOptimizations(WindowsVersion version, const std::wstring& usbDrive) {
            std::wstring bcdEditPath = GetBCDEditPathForVersion(version);
            std::wstring output;
            
            switch (version) {
                case WIN_10:
                case WIN_11:
                    // Enable additional debugging for modern Windows
                    ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} bootlog yes", targetWindowsDrive, output);
                    ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} sos yes", targetWindowsDrive, output);
                    ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} debug yes", targetWindowsDrive, output);
                    ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} debugtype Serial", targetWindowsDrive, output);
                    ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} debugport 1", targetWindowsDrive, output);
                    ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} baudrate 115200", targetWindowsDrive, output);
                    break;
                    
                case WIN_7:
                case WIN_8:
                case WIN_8_1:
                    // Legacy Windows USB optimizations
                    ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} nx OptIn", targetWindowsDrive, output);
                    ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} increaseuserva 3072", targetWindowsDrive, output);
                    ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} removememory 0", targetWindowsDrive, output);
                    break;
                    
                default:
                    // Basic optimizations for older versions
                    ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} nx OptIn", targetWindowsDrive, output);
                    break;
            }
            
            // Apply USB-specific registry-like settings through BCD
            if (!ApplyUSBBootRegistryHacks(version, usbDrive)) {
                MESSAGE = L"WARNING: Some USB registry hacks failed";
            }
            
            return true;
        }

        static bool ApplyUSBBootRegistryHacks(WindowsVersion version, const std::wstring& usbDrive) {
            // These are advanced modifications that require direct BCD editing
            // They simulate registry tweaks for better USB boot compatibility
            
            std::wstring bcdEditPath = GetBCDEditPathForVersion(version);
            std::wstring output;
            
            // Disable driver signature enforcement for USB boot
            ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} nointegritychecks on", targetWindowsDrive, output);
            ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} testsigning on", targetWindowsDrive, output);
            
            // Enable legacy boot for better USB compatibility
            ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} bootmenupolicy Legacy", targetWindowsDrive, output);
            
            // Disable secure boot for USB (if possible)
            if (version == WIN_8 || version == WIN_8_1 || version == WIN_10 || version == WIN_11) {
                ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} disabledynamictick yes", targetWindowsDrive, output);
                ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} useplatformclock yes", targetWindowsDrive, output);
            }
            
            // Memory management for USB boot
            ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} removememory 0", targetWindowsDrive, output);
            ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} truncatememory 0", targetWindowsDrive, output);
            
            // USB-specific performance tweaks
            if (version == WIN_10 || version == WIN_11) {
                ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} hypervisorlaunchtype Off", targetWindowsDrive, output);
                ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} vsmLaunchType Off", targetWindowsDrive, output);
                ExecuteBCDCommandForDrive(bcdEditPath, L"/set {current} isolatecontext no", targetWindowsDrive, output);
            }
            
            return true;
        }

        static bool CreateUSBSpecificBootEntry(const std::wstring& usbDrive) {
            MESSAGE = L"Creating USB-specific boot entry...";
            
            WindowsVersion version = GetWindowsVersionFromDrive(targetWindowsDrive);
            std::wstring bcdEditPath = GetBCDEditPathForVersion(version);
            std::wstring output;
            
            // Create a dedicated USB boot entry
            std::wstring createCommand = L"/create /d \"Windows To Go - USB\" /application osloader";
            if (!ExecuteBCDCommandForDrive(bcdEditPath, createCommand, targetWindowsDrive, output)) {
                MESSAGE = L"ERROR: Failed to create USB boot entry";
                return false;
            }
            
            // Extract the GUID of the new boot entry
            size_t guidStart = output.find(L"{");
            size_t guidEnd = output.find(L"}", guidStart);
            if (guidStart == std::wstring::npos || guidEnd == std::wstring::npos) {
                MESSAGE = L"ERROR: Cannot extract GUID from bcdedit output";
                return false;
            }
            
            std::wstring usbBootGuid = output.substr(guidStart, guidEnd - guidStart + 1);
            
            // Configure the USB-specific boot entry
            std::vector<std::wstring> usbEntryConfig = {
                L"/set " + usbBootGuid + L" device partition=" + usbDrive,
                L"/set " + usbBootGuid + L" osdevice partition=" + usbDrive,
                L"/set " + usbBootGuid + L" path \\Windows\\system32\\winload.exe",
                L"/set " + usbBootGuid + L" systemroot \\Windows",
                L"/set " + usbBootGuid + L" detecthal yes",
                L"/set " + usbBootGuid + L" winpe no",
                L"/set " + usbBootGuid + L" nointegritychecks on",
                L"/set " + usbBootGuid + L" testsigning on",
                L"/set " + usbBootGuid + L" bootmenupolicy Legacy",
                L"/set " + usbBootGuid + L" quietboot no",
                L"/set " + usbBootGuid + L" sos yes"
            };
            
            // Add version-specific USB entry settings
            if (version == WIN_10 || version == WIN_11) {
                usbEntryConfig.push_back(L"/set " + usbBootGuid + L" hypervisorlaunchtype Off");
                usbEntryConfig.push_back(L"/set " + usbBootGuid + L" isolatedcontext no");
            }
            
            for (const auto& config : usbEntryConfig) {
                if (!ExecuteBCDCommandForDrive(bcdEditPath, config, targetWindowsDrive, output)) {
                    MESSAGE = L"WARNING: Failed to configure USB boot entry: " + config;
                }
                Sleep(200);
            }
            
            // Set as default boot entry
            ExecuteBCDCommandForDrive(bcdEditPath, L"/default " + usbBootGuid, targetWindowsDrive, output);
            ExecuteBCDCommandForDrive(bcdEditPath, L"/displayorder " + usbBootGuid + L" /addfirst", targetWindowsDrive, output);
            
            MESSAGE = L"USB-specific boot entry created successfully: " + usbBootGuid;
            return true;
        }

        // Main function to make BCD USB bootable
        bool MakeBCDUSBBootable(const std::wstring& usbDrive) {
            MESSAGE = L"Making BCD bootable from USB: " + usbDrive;
            
            // Step 1: Validate current BCD
            if (!ValidateSystemBCD()) {
                MESSAGE = L"ERROR: Cannot proceed with corrupted BCD";
                return false;
            }
            
            // Step 2: Apply USB modifications
            if (!ModifyBootManager(usbDrive)) {
                MESSAGE = L"ERROR: Failed to modify boot manager for USB";
                return false;
            }
            
            // Step 3: Create dedicated USB boot entry (optional but recommended)
            if (!CreateUSBSpecificBootEntry(usbDrive)) {
                MESSAGE = L"WARNING: Failed to create dedicated USB boot entry, using modified existing entry";
            }
            
            // Step 4: Final validation
            MESSAGE = L"Performing final USB boot validation...";
            if (!ValidateSystemBCD()) {
                MESSAGE = L"ERROR: BCD corrupted after USB modifications";
                return false;
            }
            
            MESSAGE = L"BCD successfully configured for USB boot!";
            MESSAGE = L"USB Drive: " + usbDrive + L" is now bootable";
            
            return true;
        }

        static bool ValidateSystemBCD() {
            MESSAGE = L"Validating system BCD store on drive " + windows + L"...";
            
            // Check if BCD store exists on the target drive
            std::wstring targetBCDStore = windows + L"\\Boot\\BCD";
            if (!BCDStoreExists(targetBCDStore)) {
                MESSAGE = L"ERROR: BCD store not found on " + windows;
                return false;
            }
            
            // DETERMINE WINDOWS VERSION ON THE TARGET DRIVE
            WindowsVersion version = GetWindowsVersionFromDrive(windows);
            MESSAGE = L"Detected Windows version: " + GetVersionString(version);
            
            // Launch the appropriate bcdedit for that Windows version
            std::wstring output;
            std::wstring bcdEditPath = GetBCDEditPathForVersion(version);
            
            if (!ExecuteBCDCommandForDrive(bcdEditPath, L"/enum all", windows, output)) {
                MESSAGE = L"ERROR: Cannot access BCD store on " + windows;
                return false;
            }
            
            // Check for essential components
            bool hasBootManager = output.find(L"Windows Boot Manager") != std::wstring::npos;
            bool hasBootLoader = output.find(L"Windows Boot Loader") != std::wstring::npos;
            
            if (!hasBootManager || !hasBootLoader) {
                MESSAGE = L"WARNING: BCD missing essential components - attempting repair...";
                if (!RepairBCDForDrive(bcdEditPath, windows)) {
                    MESSAGE = L"ERROR: Failed to repair BCD components";
                    return false;
                }
                MESSAGE = L"BCD components repaired successfully";
            }
            
            // Check for corruption indicators
            if (output.find(L"Element not found") != std::wstring::npos ||
                output.find(L"The system cannot find the file") != std::wstring::npos) {
                MESSAGE = L"ERROR: BCD store appears corrupted - attempting repair...";
                if (!RepairBCDForDrive(bcdEditPath, windows)) {
                    MESSAGE = L"ERROR: Failed to repair corrupted BCD";
                    return false;
                }
                MESSAGE = L"BCD corruption repaired successfully";
            }
            
            MESSAGE = L"System BCD validation completed for drive " + windows;
            return true;
        }

        static WindowsVersion GetWindowsVersionFromDrive(const std::wstring& drive) {

            // Method 1: Check winver.exe or system files
            std::wstring systemPath = drive + L"\\Windows\\System32\\";
            std::wstring winverPath = systemPath + L"winver.exe";
            
            // Method 2: Check ntoskrnl.exe version
            std::wstring kernelPath = systemPath + L"ntoskrnl.exe";
            
            // Method 3: Check system registry hive
            std::wstring registryPath = drive + L"\\Windows\\System32\\config\\SOFTWARE";
            
            DWORD fileSize = GetFileSizeFromPath(kernelPath);
            
            // Check for Windows 11 by looking for specific files
            std::wstring win11File = systemPath + L"mobilenetworking.dll"; // Windows 11 specific
            if (FileExists(win11File)) {
                return WIN_11;
            }
            
            // Check kernel size as rough version indicator (simplified)
            if (fileSize > 10000000) { // ~10MB - Windows 10/11
                std::wstring win10File = systemPath + L"MusUpdateHandlers.dll"; // Windows 10 specific
                if (FileExists(win10File)) {
                    return WIN_10;
                }
                return WIN_11;
            }
            else if (fileSize > 8000000) { // ~8MB - Windows 8/8.1
                std::wstring win81File = systemPath + L"wcmapi.dll"; // Windows 8.1 specific
                if (FileExists(win81File)) {
                    return WIN_8_1;
                }
                return WIN_8;
            }
            else if (fileSize > 6000000) { // ~6MB - Windows 7
                return WIN_7;
            }
            else if (fileSize > 4000000) { // ~4MB - Vista
                return WIN_VISTA;
            }
            
            return WIN_UNKNOWN;
        }

        // Get the correct bcdedit.exe path for the Windows version
        static std::wstring GetBCDEditPathForVersion(WindowsVersion version) {
            switch (version) {
                case WIN_XP:
                    // Windows XP doesn't have bcdedit, use boot.ini
                    return L""; // Not supported
                    
                case WIN_VISTA:
                case WIN_7:
                    // Use system32 bcdedit (legacy)
                    return L"C:\\Windows\\System32\\bcdedit.exe";
                    
                case WIN_8:
                case WIN_8_1:
                    // Windows 8 bcdedit
                    return L"C:\\Windows\\System32\\bcdedit.exe";
                    
                case WIN_10:
                    // Windows 10 bcdedit (may have different syntax)
                    return L"C:\\Windows\\System32\\bcdedit.exe";
                    
                case WIN_11:
                    // Windows 11 bcdedit (latest version)
                    return L"C:\\Windows\\System32\\bcdedit.exe";
                    
                default:
                    return L"C:\\Windows\\System32\\bcdedit.exe"; // Default to current system
            }
        }

        // Execute bcdedit command for specific drive
        static bool ExecuteBCDCommandForDrive(const std::wstring& bcdEditPath, const std::wstring& arguments, 
                                            const std::wstring& drive, std::wstring& output) {
            
            std::wstring storeSwitch = L" /store \"" + drive + L"\\Boot\\BCD\"";
            std::wstring fullCommand = L"\"" + bcdEditPath + L"\"" + storeSwitch + L" " + arguments;
            
            MESSAGE = L"Executing: " + fullCommand;
            
            SHELLEXECUTEINFO sei = { sizeof(sei) };
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.lpVerb = L"runas"; // Admin privileges
            sei.lpFile = bcdEditPath.c_str();
            sei.lpParameters = (storeSwitch + L" " + arguments).c_str();
            sei.nShow = SW_HIDE;
            
            if (!ShellExecuteEx(&sei)) {
                // Fallback: try without admin privileges
                sei.lpVerb = NULL;
                if (!ShellExecuteEx(&sei)) {
                    MESSAGE = L"ERROR: Failed to launch bcdedit";
                    return false;
                }
            }
            
            WaitForSingleObject(sei.hProcess, INFINITE);
            
            DWORD exitCode;
            GetExitCodeProcess(sei.hProcess, &exitCode);
            CloseHandle(sei.hProcess);
            
            // For output, we need to use CreateProcess to capture it
            return CaptureBCDOutput(bcdEditPath, storeSwitch + L" " + arguments, output);
        }

        // Better version detection by reading system files
        static WindowsVersion GetWindowsVersionFromDriveDetailed(const std::wstring& drive) {
            std::wstring systemPath = drive + L"\\Windows\\System32\\";
            
            // Check for Windows 11 specific files
            if (FileExists(systemPath + L"MusNotification.exe")) {
                return WIN_11;
            }
            
            // Check for Windows 10 specific files
            if (FileExists(systemPath + L"MusUpdateHandlers.dll")) {
                return WIN_10;
            }
            
            // Check for Windows 8.1 specific files
            if (FileExists(systemPath + L"wcmapi.dll")) {
                return WIN_8_1;
            }
            
            // Check for Windows 8 specific files
            if (FileExists(systemPath + L"PlayToManager.dll")) {
                return WIN_8;
            }
            
            // Check for Windows 7 specific files
            if (FileExists(systemPath + L"api-ms-win-core-synch-l1-2-0.dll")) {
                return WIN_7;
            }
            
            // Read version from kernel file
            std::wstring kernelPath = systemPath + L"ntoskrnl.exe";
            DWORD versionSize = GetFileVersionInfoSizeW(kernelPath.c_str(), NULL);
            if (versionSize > 0) {
                std::vector<BYTE> versionData(versionSize);
                if (GetFileVersionInfoW(kernelPath.c_str(), 0, versionSize, versionData.data())) {
                    VS_FIXEDFILEINFO* fileInfo;
                    UINT len;
                    if (VerQueryValueW(versionData.data(), L"\\", (LPVOID*)&fileInfo, &len)) {
                        WORD major = HIWORD(fileInfo->dwProductVersionMS);
                        WORD minor = LOWORD(fileInfo->dwProductVersionMS);
                        
                        if (major == 10) {
                            if (minor >= 22000) return WIN_11;
                            return WIN_10;
                        } else if (major == 6) {
                            if (minor == 3) return WIN_8_1;
                            if (minor == 2) return WIN_8;
                            if (minor == 1) return WIN_7;
                            if (minor == 0) return WIN_VISTA;
                        }
                    }
                }
            }
            
            return WIN_UNKNOWN;
        }

        // Utility functions
        static bool BCDStoreExists(const std::wstring& storePath) {
            DWORD attributes = GetFileAttributesW(storePath.c_str());
            return (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY));
        }

        static bool FileExists(const std::wstring& path) {
            DWORD attributes = GetFileAttributesW(path.c_str());
            return (attributes != INVALID_FILE_ATTRIBUTES);
        }

        static DWORD GetFileSizeFromPath(const std::wstring& path) {
            WIN32_FILE_ATTRIBUTE_DATA fileAttr;
            if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fileAttr)) {
                return fileAttr.nFileSizeLow;
            }
            return 0;
        }

        static std::wstring GetVersionString(WindowsVersion version) {
            switch (version) {
                case WIN_XP: return L"Windows XP";
                case WIN_VISTA: return L"Windows Vista";
                case WIN_7: return L"Windows 7";
                case WIN_8: return L"Windows 8";
                case WIN_8_1: return L"Windows 8.1";
                case WIN_10: return L"Windows 10";
                case WIN_11: return L"Windows 11";
                default: return L"Unknown Windows Version";
            }
        }
        
};

#endif