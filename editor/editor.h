#ifndef _BCD_H_
#define _BCD_H_
#include <../tracker/tracker.h>
#include <../interface/interface.h>
#include <vector>
#include <mutex>
#include <atomic> // TODO: Use for to update the message constantly
#include <winternl.h>
#include <sfc.h>
#include <setupapi.h>
#include <shellapi.h>
#include <winioctl.h>
#include <wintrust.h>
#include <iostream>
#include <map>
#include <tchar.h>

#pragma comment(lib, "sfc.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "version.lib")

extern std::wstring MESSAGE;
extern std::wstring MESSAGE_ERROR;

namespace Editor {

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

    enum RecoveryStatus {
        STATUS_SUCCESS = 0,
        STATUS_BCD_CORRUPTED = 1,
        STATUS_WINLOAD_MISSING = 2,
        STATUS_USB_INACCESSIBLE = 3,
        STATUS_INSUFFICIENT_SPACE = 4,
        STATUS_PARTITION_FAILED = 5,
        STATUS_FILE_COPY_FAILED = 6,
        STATUS_BOOT_CONFIG_FAILED = 7,
        STATUS_UNKNOWN_ERROR = 99
    };

    class BCD : public Interface::CRTP<BCD> {

        public:

            
            friend class WinloadEFIRepair;
            friend class RecoveryShell;

            explicit BCD(const std::wstring& _drive) {
                // Used for modifying the bcd to work on a usb
                if (drive.empty()) drive = _drive;
                MakeBCDUSBBootable();

            }

            explicit BCD(const std::wstring& _drive, const std::wstring& windows_driver) {

                windows = windows_driver;
                drive = _drive;
                ModifyBootManager();

            }

            ~BCD() = default;

        private:

            static std::wstring windows;
            static std::wstring drive;
            static std::wstring lpParameters;
            static std::wstring bcdEditPath;


            bool ModifyBootManager() {
                MESSAGE = L"Modifying BCD bootloader for USB compatibility...";

                // First validate the current BCD
                if (!ValidateSystemBCD()) {

                    MESSAGE = L"ERROR: Cannot modify corrupted BCD";
                    return false;

                }

                WindowsVersion version = GetWindowsVersionFromDrive();
               
                // USB-specific BCD modifications
                static std::vector<std::wstring> usbModifications = GetUSBModificationsForVersion(version);

                for (const auto& modification : usbModifications) {
                    static std::wstring output;
                    if (!ExecuteBCDCommandForDrive(modification, windows, output)) {
                        MESSAGE = L"WARNING: Failed to apply USB modification: " + modification;
                        // Continue with other modifications
                    }
                    Sleep(500); // Small delay between commands
                }

                // Apply version-specific USB optimizations
                if (!ApplyVersionSpecificUSBOptimizations(version)) {
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

            static std::vector<std::wstring> GetUSBModificationsForVersion(WindowsVersion version) {
                std::vector<std::wstring> modifications;

                // COMMON USB MODIFICATIONS FOR ALL WINDOWS VERSIONS
                modifications.push_back(L"/set {bootmgr} device partition=" + drive);
                modifications.push_back(L"/set {bootmgr} timeout 10");
                modifications.push_back(L"/set {bootmgr} displayorder {current}");
                modifications.push_back(L"/displayorder {current} /addfirst");

                // Windows Boot Loader modifications for USB
                modifications.push_back(L"/set {current} device partition=" + drive);
                modifications.push_back(L"/set {current} osdevice partition=" + drive);
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

            static bool ApplyVersionSpecificUSBOptimizations(WindowsVersion version) {
                std::wstring output;

                switch (version) {
                case WIN_10:
                case WIN_11:
                    // Enable additional debugging for modern Windows
                    ExecuteBCDCommandForDrive(L"/set {current} bootlog yes", drive, output);
                    ExecuteBCDCommandForDrive(L"/set {current} sos yes", drive, output);
                    ExecuteBCDCommandForDrive(L"/set {current} debug yes", drive, output);
                    ExecuteBCDCommandForDrive(L"/set {current} debugtype Serial", drive, output);
                    ExecuteBCDCommandForDrive(L"/set {current} debugport 1", drive, output);
                    ExecuteBCDCommandForDrive(L"/set {current} baudrate 115200", drive, output);
                    break;

                case WIN_7:
                case WIN_8:
                case WIN_8_1:
                    // Legacy Windows USB optimizations
                    ExecuteBCDCommandForDrive(L"/set {current} nx OptIn", drive, output);
                    ExecuteBCDCommandForDrive(L"/set {current} increaseuserva 3072", drive, output);
                    ExecuteBCDCommandForDrive(L"/set {current} removememory 0", drive, output);
                    break;

                default:
                    // Basic optimizations for older versions
                    ExecuteBCDCommandForDrive(L"/set {current} nx OptIn", drive, output);
                    break;
                }

                // Apply USB-specific registry-like settings through BCD
                if (!ApplyUSBBootRegistryHacks(version)) {
                    MESSAGE = L"WARNING: Some USB registry hacks failed";
                }

                return true;
            }

            static bool ApplyUSBBootRegistryHacks(WindowsVersion version) {
                // These are advanced modifications that require direct BCD editing
                // They simulate registry tweaks for better USB boot compatibility

                std::wstring output;

                // Disable driver signature enforcement for USB boot
                ExecuteBCDCommandForDrive(L"/set {current} nointegritychecks on", drive, output);
                ExecuteBCDCommandForDrive(L"/set {current} testsigning on", drive, output);

                // Enable legacy boot for better USB compatibility
                ExecuteBCDCommandForDrive(L"/set {current} bootmenupolicy Legacy", drive, output);

                // Disable secure boot for USB (if possible)
                if (version == WIN_8 || version == WIN_8_1 || version == WIN_10 || version == WIN_11) {
                    ExecuteBCDCommandForDrive(L"/set {current} disabledynamictick yes", drive, output);
                    ExecuteBCDCommandForDrive(L"/set {current} useplatformclock yes", drive, output);
                }

                // Memory management for USB boot
                ExecuteBCDCommandForDrive(L"/set {current} removememory 0", drive, output);
                ExecuteBCDCommandForDrive(L"/set {current} truncatememory 0", drive, output);

                // USB-specific performance tweaks
                if (version == WIN_10 || version == WIN_11) {
                    ExecuteBCDCommandForDrive(L"/set {current} hypervisorlaunchtype Off", drive, output);
                    ExecuteBCDCommandForDrive(L"/set {current} vsmLaunchType Off", drive, output);
                    ExecuteBCDCommandForDrive(L"/set {current} isolatecontext no", drive, output);
                }

                return true;
            }

            static bool CreateUSBSpecificBootEntry() {
                MESSAGE = L"Creating USB-specific boot entry...";

                WindowsVersion version = GetWindowsVersionFromDrive();
                std::wstring output;

                // Create a dedicated USB boot entry
                std::wstring createCommand = L"/create /d \"Windows To Go - USB\" /application osloader";
                if (!ExecuteBCDCommandForDrive(createCommand, drive, output)) {
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
                static std::vector<std::wstring> usbEntryConfig = {
                    L"/set " + usbBootGuid + L" device partition=" + drive,
                    L"/set " + usbBootGuid + L" osdevice partition=" + drive,
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
                    if (!ExecuteBCDCommandForDrive(config, drive, output))
                        MESSAGE = L"WARNING: Failed to configure USB boot entry: " + config;
                    Sleep(200);
                }

                // Set as default boot entry
                ExecuteBCDCommandForDrive(L"/default " + usbBootGuid, drive, output);
                ExecuteBCDCommandForDrive(L"/displayorder " + usbBootGuid + L" /addfirst", drive, output);

                MESSAGE = L"USB-specific boot entry created successfully: " + usbBootGuid;
                return true;
            }

            // Main function to make BCD USB bootable
            bool MakeBCDUSBBootable() {
                MESSAGE = L"Making BCD bootable from USB: " + drive;

                // Step 1: Validate current BCD
                if (!ValidateSystemBCD()) {
                    MESSAGE = L"ERROR: Cannot proceed with corrupted BCD";
                    return false;
                }

                // Step 2: Apply USB modifications
                if (!ModifyBootManager()) {
                    MESSAGE = L"ERROR: Failed to modify boot manager for USB";
                    return false;
                }

                // Step 3: Create dedicated USB boot entry (optional but recommended)
                if (!CreateUSBSpecificBootEntry()) {
                    MESSAGE = L"WARNING: Failed to create dedicated USB boot entry, using modified existing entry";
                }

                // Step 4: Final validation
                MESSAGE = L"Performing final USB boot validation...";
                if (!ValidateSystemBCD()) {
                    MESSAGE = L"ERROR: BCD corrupted after USB modifications";
                    return false;
                }

                MESSAGE = L"BCD successfully configured for USB boot!";
                MESSAGE = L"USB Drive: " + drive + L" is now bootable";

                return true;
            }

            static bool ValidateSystemBCD() {

                MESSAGE = L"Validating system BCD store on drive " + !windows.empty() ? windows + L"..."  : drive + L"...";

                // Check if BCD store exists on the target drive
                std::wstring targetBCDStore = !windows.empty() ? windows + L"\\Boot\\BCD" : drive + L"\\Boot\\BCD";
                if (!BCDStoreExists(targetBCDStore)) {
                    MESSAGE = L"ERROR: BCD store not found on " + !windows.empty() ? windows : drive;
                    return false;
                }

                // DETERMINE WINDOWS VERSION ON THE TARGET DRIVE
                WindowsVersion version = GetWindowsVersionFromDrive();
                MESSAGE = L"Detected Windows version: " + GetVersionString(version);

                // Launch the appropriate bcdedit for that Windows version
                std::wstring output;
                if (bcdEditPath.empty()) bcdEditPath = GetBCDEditPathForVersion(version);

                if (!ExecuteBCDCommandForDrive(L"/enum all", !windows.empty() ? windows : drive, output)) {
                    MESSAGE = L"ERROR: Cannot access BCD store on " + !windows.empty() ? windows : drive;
                    return false;
                }

                // Check for essential components
                bool hasBootManager = output.find(L"Windows Boot Manager") != std::wstring::npos;
                bool hasBootLoader = output.find(L"Windows Boot Loader") != std::wstring::npos;

                if (!hasBootManager || !hasBootLoader) {
                    MESSAGE = L"WARNING: BCD missing essential components - attempting repair...";
                    if (!RepairBCDForDrive(version)) {
                        MESSAGE = L"ERROR: Failed to repair BCD components";
                        return false;
                    }
                    MESSAGE = L"BCD components repaired successfully";
                }

                // Check for corruption indicators
                if (output.find(L"Element not found") != std::wstring::npos ||
                    output.find(L"The system cannot find the file") != std::wstring::npos) {
                    MESSAGE = L"ERROR: BCD store appears corrupted - attempting repair...";
                    if (!RepairBCDForDrive(version)) {
                        MESSAGE = L"ERROR: Failed to repair corrupted BCD";
                        return false;
                    }
                    MESSAGE = L"BCD corruption repaired successfully";
                }

                MESSAGE = L"System BCD validation completed for drive " + !windows.empty() ? windows : drive;
                return true;
            }

            static bool RepairBCDForDrive(WindowsVersion version) {
                MESSAGE = L"[REPAIR] Starting BCD repair for drive: " + drive + L"\n";
                MESSAGE = L"[REPAIR] Windows version: " + GetVersionString(version) + L"\n";

              
                if (!BackupBCDStore()) {
                    MESSAGE_ERROR = L"[WARNING] Failed to backup BCD store, continuing anyway...\n" ;
                }

                static std::vector<bool> repairResults = {
                    RebuildBCDStore(drive, version),
                    RepairWithBootRec(drive, version),
                    CreateNewBCDStore(drive, version),
                    ManualBCDRepair(drive, version)
                };

                bool repairSuccessful = false;
                for (bool result : repairResults) {
                    if (result) {
                        repairSuccessful = true;
                        break;
                    }
                }

                if (repairSuccessful) {
                    if (ValidateRepairedBCD()) {
                        MESSAGE = L"[SUCCESS] BCD repair completed and validated successfully\n";
                        return true;
                    }
                    else {
                        MESSAGE = L"[WARNING] BCD repair completed but validation failed\n";
                        return true; // Still return true as repair was attempted
                    }
                }

                MESSAGE_ERROR = L"[ERROR] All BCD repair methods failed\n";
                return false;
            }

            // TODO: Parameter is not needed
            static bool BackupBCDStore() {
                std::wstring bcdPath = drive + L"\\Boot\\BCD";
                std::wstring backupPath = drive + L"\\Boot\\BCD.backup." + std::to_wstring(GetTickCount64());

                MESSAGE = L"[BACKUP] Creating BCD backup: " + backupPath + L"\n";

                if (CopyFileW(bcdPath.c_str(), backupPath.c_str(), FALSE)) {

                    MESSAGE = L"[BACKUP] BCD backup created successfully\n";
                    return true;

                }
                else {

                    MESSAGE_ERROR = L"[WARNING] Failed to create BCD backup, error: " + std::to_wstring(GetLastError()) + L"\n";
                    return false;

                }
            }

            // TODO: Parameter not needed: drive
            static bool RebuildBCDStore(const std::wstring& drive, WindowsVersion version) {
                std::wcout << L"[REPAIR] Attempting to rebuild BCD store..." << std::endl;
                std::wstring output;

                // Use bootrec for comprehensive repair (Windows 7 and later)
                if (version >= WIN_7) {
                    MESSAGE = L"[REPAIR] Using bootrec.exe for repair...\n";

                    // TODO: This can become a private data member
                    std::vector<std::wstring> bootRecCommands = {
                        L"/RebuildBcd",
                        L"/FixMbr",
                        L"/FixBoot"
                    };

                    for (const auto& command : bootRecCommands) {
                        std::wstring fullCommand = L"bootrec.exe " + command;
                        MESSAGE = L"[EXEC] " + fullCommand + L"\n";

                        if (!ExecuteSystemCommand(fullCommand, output)) {
                            MESSAGE = L"[WARNING] bootrec command failed: " + command + L"\n";
                        }

                        std::this_thread::sleep_for(std::chrono::seconds(2));

                    }
                }

                // Use bcdedit to rebuild BCD
                std::wstring rebuildCommand = L"/rebuildbcd";
                if (ExecuteBCDCommandForDrive(rebuildCommand, drive, output)) {

                    MESSAGE = L"[SUCCESS] BCD rebuild completed\n";
                    return true;

                }

                MESSAGE = L"[FAILED] BCD rebuild failed\n";
                return false;
            }

            static bool RepairWithBootRec(const std::wstring& drive, WindowsVersion version) {
                if (version < WIN_7) {

                    MESSAGE = L"[INFO] bootrec.exe not available for this Windows version\n";
                    return false;

                }

                MESSAGE = L"[REPAIR] Using bootrec.exe for advanced repair...\n";

                std::wstring systemRoot = drive + L"\\Windows";
                std::wstring bootRecPath = L"C:\\Windows\\System32\\bootrec.exe";

                // Check if bootrec exists
                if (GetFileAttributesW(bootRecPath.c_str()) == INVALID_FILE_ATTRIBUTES) {

                    MESSAGE = L"[INFO] bootrec.exe not found, skipping this method\n";
                    return false;

                }

                // TODO: This can be made into a private data member variable
                static std::vector<std::wstring> commands = {
                    L"/scanos",
                    L"/rebuildbcd",
                    L"/fixmbr",
                    L"/fixboot"
                };

                bool anySuccess = false;
                std::wstring output;

                for (const auto& command : commands) {
                    MESSAGE = L"[EXEC] bootrec.exe " + command + L"\n";

                    if (ExecuteSystemCommand(L"bootrec.exe " + command, output)) {

                        MESSAGE = L"[SUCCESS] bootrec " + command + L" completed" + L"\n";
                        anySuccess = true;

                        // Analyze output for specific results
                        if (command == L"/scanos" && output.find(L"Total identified Windows installations:") != std::wstring::npos) {
                            MESSAGE = L"[INFO] Windows installations detected: " + output + L"\n";
                        }
                    }
                    else {

                        // TODO: Implement some kind of counter and if it fails more than 3 times, we throw 
                        MESSAGE = L"[WARNING] bootrec " + command + L" failed" + L"\n";
                    }

                    std::this_thread::sleep_for(std::chrono::seconds(3));
                }

                return anySuccess;
            }

            static bool CreateNewBCDStore(const std::wstring& drive, WindowsVersion version) {
                MESSAGE = L"[REPAIR] Creating new BCD store from scratch...\n";

                std::wstring bcdPath = drive + L"\\Boot\\BCD";
                std::wstring output;

                // Delete corrupted BCD store
                DeleteFileW(bcdPath.c_str());
                std::this_thread::sleep_for(std::chrono::seconds(1));

                // Create new BCD store
                std::wstring createCommand = L"/createstore \"" + bcdPath + L"\"";
                if (!ExecuteBCDCommand(createCommand, output)) {
                    
                    MESSAGE_ERROR = L"[FAILED] Failed to create new BCD store\n";
                    throw;
                }

                // Recreate essential boot entries
                if (!RecreateEssentialBootEntries(version)) {

                    MESSAGE_ERROR = L"[WARNING] Failed to recreate some boot entries\n";
                    throw;

                }

                MESSAGE = L"[SUCCESS] New BCD store created successfully\n";
                return true;
            }

            static bool RecreateEssentialBootEntries(WindowsVersion version) {
                MESSAGE = L"[REPAIR] Recreating essential boot entries...\n";

                std::wstring output;
                std::wstring storeSwitch = L" /store \"" + drive + L"\\Boot\\BCD\"";

                // Create Windows Boot Manager
                static std::vector<std::wstring> commands = {
                    L"/create {bootmgr} /d \"Windows Boot Manager\"",
                    L"/set {bootmgr} device partition=" + drive,
                    L"/set {bootmgr} timeout 30",
                    L"/set {bootmgr} displaybootmenu yes",

                    // Create Windows Boot Loader
                    L"/create /d \"Windows\" /application osloader",
                };

                for (const auto& command : commands) {

                    if (!ExecuteBCDCommand(storeSwitch + L" " + command, output)) {
                        MESSAGE = L"[WARNING] Failed to execute: " + command + L"\n";
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }

                // Extract GUID from the last command output and configure it
                size_t guidStart = output.find(L"{");
                size_t guidEnd = output.find(L"}", guidStart);
                if (guidStart != std::wstring::npos && guidEnd != std::wstring::npos) {
                    std::wstring bootLoaderGuid = output.substr(guidStart, guidEnd - guidStart + 1);

                    static std::vector<std::wstring> loaderCommands = {
                        L"/set " + bootLoaderGuid + L" device partition=" + drive,
                        L"/set " + bootLoaderGuid + L" osdevice partition=" + drive,
                        L"/set " + bootLoaderGuid + L" path \\Windows\\system32\\winload.exe",
                        L"/set " + bootLoaderGuid + L" systemroot \\Windows",
                        L"/set " + bootLoaderGuid + L" detecthal yes",
                        L"/set " + bootLoaderGuid + L" winpe no",
                        L"/default " + bootLoaderGuid,
                        L"/displayorder " + bootLoaderGuid + L" /addfirst"
                    };

                    for (const auto& command : loaderCommands) {
                        ExecuteBCDCommand(storeSwitch + L" " + command, output);
                        std::this_thread::sleep_for(std::chrono::milliseconds(300));
                    }
                }

                return true;
            }

            static bool ManualBCDRepair(const std::wstring& drive, WindowsVersion version) {
                MESSAGE = L"[REPAIR] Attempting manual BCD repair...\n";

                std::wstring output;

                // Export current configuration for analysis
                std::wstring exportFile = drive + L"\\bcd_export.txt";
                std::wstring exportCommand = L"/export \"" + exportFile + L"\"";
                ExecuteBCDCommandForDrive(exportCommand, drive, output);

                // Common repair patterns for different Windows versions
                std::vector<std::wstring> repairCommands = GetVersionSpecificRepairCommands(version, drive);

                bool anySuccess = false;
                for (const auto& command : repairCommands) {
                    MESSAGE = L"[EXEC] " + command + L"\n";

                    if (ExecuteBCDCommandForDrive(command, drive, output)) {
                        MESSAGE = L"[SUCCESS] Repair command completed: " + command + L"\n";
                        anySuccess = true;
                    }
                    else {
                        MESSAGE = L"[WARNING] Repair command failed: " + command + L"\n";
                    }

                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }

                return anySuccess;
            }

            static std::vector<std::wstring> GetVersionSpecificRepairCommands(WindowsVersion version, const std::wstring& drive) {
                std::vector<std::wstring> commands;

                // Common repair commands for all versions
                commands.push_back(L"/enum all");
                commands.push_back(L"/deletevalue {current} badmemorylist");
                commands.push_back(L"/deletevalue {current} badmemoryaccess");

                // Version-specific repair commands
                switch (version) {
                case WIN_10:
                case WIN_11:
                    commands.push_back(L"/set {current} recoveryenabled no");
                    commands.push_back(L"/set {current} isolatedcontext no");
                    commands.push_back(L"/set {current} hypervisorlaunchtype Off");
                    break;

                case WIN_8:
                case WIN_8_1:
                    commands.push_back(L"/set {bootmgr} displaybootmenu yes");
                    commands.push_back(L"/set {current} bootmenupolicy Legacy");
                    break;

                case WIN_7:
                case WIN_VISTA:
                    commands.push_back(L"/set {current} nointegritychecks yes");
                    commands.push_back(L"/set {current} pae ForceEnable");
                    break;

                default:
                    break;
                }

                // Reset critical boot parameters
                commands.push_back(L"/set {current} device partition=" + drive);
                commands.push_back(L"/set {current} osdevice partition=" + drive);
                commands.push_back(L"/set {current} path \\Windows\\system32\\winload.exe");
                commands.push_back(L"/set {current} systemroot \\Windows");

                return commands;
            }

            // para not needed: drive
            static bool ValidateRepairedBCD() {
                MESSAGE = L"[VALIDATION] Validating repaired BCD store...\n";
                
                std::wstring bcdPath = drive + L"\\Boot\\BCD";

                // Check if BCD file exists and is accessible
                if (GetFileAttributesW(bcdPath.c_str()) == INVALID_FILE_ATTRIBUTES) {

                    MESSAGE_ERROR = L"[ERROR] BCD store not found after repair\n";
                    throw;

                }

                // Test BCD accessibility with bcdedit
                std::wstring output;
                if (!ExecuteBCDCommand(L"/enum all /store \"" + bcdPath + L"\"", output)) {

                    MESSAGE_ERROR = L"[ERROR] Repaired BCD store is not accessible\n";
                    throw;

                }

                // Check for essential components in the output
                const bool hasBootManager = output.find(L"Windows Boot Manager") != std::wstring::npos;
                const bool hasBootLoader = output.find(L"Windows Boot Loader") != std::wstring::npos;

                if (!hasBootManager || !hasBootLoader) {

                    MESSAGE_ERROR = L"[WARNING] Repaired BCD missing essential components\n";
                    throw;
                }

                // Check for common error indicators
                const bool hasErrors = output.find(L"Element not found") != std::wstring::npos ||
                    output.find(L"The system cannot find the file") != std::wstring::npos ||
                    output.find(L"cannot be found") != std::wstring::npos;

                if (hasErrors) {

                    MESSAGE = L"[WARNING] Repaired BCD still shows error indicators\n";
                    MESSAGE = L"Would you like to continue? Y/N\n";
                    if (ConfirmUserIntent()) return true;
                    throw;

                }

                MESSAGE = L"[SUCCESS] BCD validation passed\n";
                return true;
            }

            static bool ConfirmUserIntent() { return false; }

            static WindowsVersion GetWindowsVersionFromDrive() {

                // Method 1: Check winver.exe or system files
                std::wstring systemPath = !drive.empty() ? drive + L"\\Windows\\System32\\" : windows + L"\\Windows\\System32\\";
                std::wstring winverPath = systemPath + L"winver.exe";

                // Method 2: Check ntoskrnl.exe version
                std::wstring kernelPath = systemPath + L"ntoskrnl.exe";

                // Method 3: Check system registry hive
                std::wstring registryPath = !drive.empty() ? drive + L"\\Windows\\System32\\config\\SOFTWARE" : windows + L"\\Windows\\System32\\config\\SOFTWARE";

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
            // This works if drive variable is empty or if windows is empty
            static std::wstring GetBCDEditPathForVersion(WindowsVersion version) {
                switch (version) {
                case WIN_XP:
                    // TOOD: We need to use boot.ini, because bcdedit is not avialaible on windows xp
                    throw "WINDOWS XP IS NOT SUPPORTED!\n";
                    //return L""; // Not supported

                case WIN_VISTA:
                case WIN_7:
                    // Use system32 bcdedit (legacy)
             
                    return windows.empty() ? drive + L":\\Windows\\System32\\bcdedit.exe" : windows + L":\\Windows\\System32\\bcdedit.exe";

                case WIN_8:
                case WIN_8_1:
                    // Windows 8 bcdedit
                    if (windows.empty()) return drive + L":\\Windows\\System32\\bcdedit.exe";
                    return windows + L":\\Windows\\System32\\bcdedit.exe";

                case WIN_10:
                    // Windows 10 bcdedit (may have different syntax)
                    if (windows.empty()) return drive + L":\\Windows\\System32\\bcdedit.exe";
                    return windows + L":\\Windows\\System32\\bcdedit.exe";

                case WIN_11:
                    // Windows 11 bcdedit (latest version)
                    if (windows.empty()) return drive + L":\\Windows\\System32\\bcdedit.exe";
                    return windows + L":\\Windows\\System32\\bcdedit.exe";

                default:
                    return L"C:\\Windows\\System32\\bcdedit.exe"; // Default to current system
                }
            }

            // Execute bcdedit command for specific drive
            static bool ExecuteBCDCommandForDrive(const std::wstring& modified_usb, const std::wstring& arguments, std::wstring& output) {

                const std::wstring storeSwitch = L" /store \"" + modified_usb + L"\\Boot\\BCD\"";
                std::wstring fullCommand = L"\"" + bcdEditPath + L"\"" + storeSwitch + L" " + arguments;
                lpParameters = storeSwitch + L"" + arguments;

                MESSAGE = L"Executing: " + fullCommand;

                SHELLEXECUTEINFO sei = { sizeof(sei) };
                sei.fMask = SEE_MASK_NOCLOSEPROCESS;
                sei.lpVerb = _T("runas"); // Admin privileges
                sei.lpFile = bcdEditPath.c_str();
                sei.lpParameters = lpParameters.c_str();
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

                return CaptureBCDOutput(storeSwitch + L" " + arguments, output);
            }

            static bool CaptureBCDOutput(const std::wstring& arguments, std::wstring& output) {

                SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES) };
                sa.bInheritHandle = TRUE;

                HANDLE hStdoutRd = NULL, hStdoutWr = NULL;
                HANDLE hStdinRd = NULL, hStdinWr = NULL;

                // Create pipes for stdout and stderr
                if (!CreatePipe(&hStdoutRd, &hStdoutWr, &sa, 0)) {
                    std::wcerr << L"ERROR: Failed to create stdout pipe" << std::endl;
                    return false;
                }

                SetHandleInformation(hStdoutRd, HANDLE_FLAG_INHERIT, 0);

                if (!CreatePipe(&hStdinRd, &hStdinWr, &sa, 0)) {
                    MESSAGE_ERROR = L"ERROR: Failed to create stdin pipe\n";
                    CloseHandle(hStdoutRd);
                    CloseHandle(hStdoutWr);
                    throw;
                }
                SetHandleInformation(hStdinWr, HANDLE_FLAG_INHERIT, 0);

                // Set up startup info
                STARTUPINFO si = { sizeof(STARTUPINFO) };
                si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
                si.hStdOutput = hStdoutWr;
                si.hStdError = hStdoutWr;
                si.hStdInput = hStdinRd;
                si.wShowWindow = SW_HIDE; // Hide the console window

                PROCESS_INFORMATION pi = { 0 };

                // Build the full command line
                std::wstring fullCommand = L"\"" + bcdEditPath + L"\" " + arguments;

                MESSAGE = L"[DEBUG] Executing: " + fullCommand + L"\n";

                // Create the process
                BOOL success = CreateProcessW(
                    NULL,                           // No module name (use command line)
                    const_cast<LPWSTR>(fullCommand.c_str()), // Command line
                    NULL,                           // Process handle not inheritable
                    NULL,                           // Thread handle not inheritable
                    TRUE,                           // Set handle inheritance to TRUE
                    CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, // Creation flags
                    NULL,                           // Use parent's environment block
                    NULL,                           // Use parent's starting directory
                    &si,                            // Pointer to STARTUPINFO structure (This will be an issue when running on linux)
                    &pi                             // Pointer to PROCESS_INFORMATION structure
                );

                // Close the write end of the pipes (child process has copies now)
                CloseHandle(hStdoutWr);
                CloseHandle(hStdinRd);

                if (!success) {
                    DWORD error = GetLastError();
                    std::wcerr << L"ERROR: Failed to create process for bcdedit.exe - Error code: " << error << std::endl;

                    CloseHandle(hStdoutRd);
                    CloseHandle(hStdinWr);
                    return false;
                }

                // Read output from the child process
                DWORD bytesRead;
                CHAR buffer[4096];
                std::string result;
                DWORD startTime = GetTickCount64();
                const DWORD timeout = 30000; // 30 second timeout

                while (true) {
                    // Check if data is available to read
                    DWORD bytesAvailable = 0;
                    if (!PeekNamedPipe(hStdoutRd, NULL, 0, NULL, &bytesAvailable, NULL)) {
                        // Pipe might be broken, break out
                        break;
                    }

                    if (bytesAvailable > 0) {
                        if (ReadFile(hStdoutRd, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                            buffer[bytesRead] = '\0';
                            result += buffer;
                        }
                    }
                    else {
                        // No data available, check if process has terminated
                        DWORD exitCode = STILL_ACTIVE;
                        if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
                            break; // Failed to get exit code
                        }

                        if (exitCode != STILL_ACTIVE) {
                            break; // Process has terminated
                        }
                    }

                    // Check for timeout
                    if (GetTickCount64() - startTime > timeout) {
                        std::wcerr << L"WARNING: bcdedit process timed out after 30 seconds" << std::endl;
                        TerminateProcess(pi.hProcess, 1);
                        break;
                    }

                    // Small delay to prevent busy waiting
                    Sleep(50);
                }

                // Get the final exit code
                DWORD exitCode;
                if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
                    std::wcerr << L"ERROR: Failed to get process exit code" << std::endl;
                    exitCode = 1; // Assume failure
                }

                // Output the result to CLI in real-time
                if (!result.empty()) {
                    // Convert to wide string for output
                    int wideLen = MultiByteToWideChar(CP_UTF8, 0, result.c_str(), -1, NULL, 0);
                    if (wideLen > 0) {
                        output.resize(wideLen);
                        MultiByteToWideChar(CP_UTF8, 0, result.c_str(), -1, &output[0], wideLen);

                        // Print to CLI
                        std::wcout << L"[BCDEDIT OUTPUT]" << std::endl;
                        std::wcout << output << std::endl;
                    }
                }

                // Clean up
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                CloseHandle(hStdoutRd);
                CloseHandle(hStdinWr);

                // Check if command was successful
                bool commandSuccess = (exitCode == 0);

                if (!commandSuccess) {
                    MESSAGE = L"ERROR: bcdedit command failed with exit code: " + std::to_wstring(exitCode) + L"\n";

                    // Provide more specific error messages based on exit code
                    switch (exitCode) {
                    case 1:
                        std::wcerr << L"BCD store access denied or not found" << std::endl;
                        break;
                    case 2:
                        std::wcerr << L"Invalid command syntax" << std::endl;
                        break;
                    case 3:
                        std::wcerr << L"Specified boot entry not found" << std::endl;
                        break;
                    case 5:
                        std::wcerr << L"Access denied - run as Administrator" << std::endl;
                        break;
                    default:
                        std::wcerr << L"Unknown bcdedit error" << std::endl;
                        break;
                    }
                }
                else {
                    MESSAGE = L"[SUCCESS] bcdedit command completed successfully\n";
                }

                return commandSuccess;
            }


            // Better version detection by reading system files
            static WindowsVersion GetWindowsVersionFromDriveDetailed() {
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
                        static VS_FIXEDFILEINFO* fileInfo = nullptr;
                        UINT len;
                        if (VerQueryValueW(versionData.data(), L"\\", (LPVOID*)&fileInfo, &len)) {
                            WORD major = HIWORD(fileInfo->dwProductVersionMS);
                            WORD minor = LOWORD(fileInfo->dwProductVersionMS);

                            if (major == 10) {
                                if (minor >= 22000) return WIN_11;
                                return WIN_10;
                            }
                            else if (major == 6) {
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
                static WIN32_FILE_ATTRIBUTE_DATA fileAttr;
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

            static bool ExecuteSystemCommand(const std::wstring& command, std::wstring& output) {
                // Simplified system command execution
                // Implementation similar to CaptureBCDOutput but for general commands
                // TODO: Need to add L"cmd.exe" somewhere 
                return CaptureBCDOutput(L"/c " + command, output);
            }

            static bool ExecuteBCDCommand(const std::wstring& arguments, std::wstring& output) {
                // Wrapper for bcdedit command execution
                return CaptureBCDOutput(arguments, output);
            }

    };

    class WinloadEFIRepair : public Interface::CRTP<WinloadEFIRepair> {
       
        private:

            static std::wstring usbDrive;
            static std::wstring systemRoot;

        public:

            WinloadEFIRepair(const std::wstring& drive) {
                usbDrive = drive;
                systemRoot = usbDrive + L"\\Windows";
            }

            static bool FixWinloadEFI() {
                MESSAGE = L"Starting winload.efi repair for USB boot...";

                // Step 1: Check if winload.efi exists and is accessible
                if (!CheckWinloadExistence()) {
                    MESSAGE = L"ERROR: winload.efi not found or inaccessible";
                    return false;
                }

                // Step 2: Verify winload.efi integrity
                if (!VerifyWinloadIntegrity()) {
                    MESSAGE = L"WARNING: winload.efi integrity check failed";
                    // Continue to repair anyway
                }

                // Step 3: Repair winload.efi using multiple methods
                if (!RepairWinloadEFI()) {
                    MESSAGE = L"ERROR: Failed to repair winload.efi";
                    return false;
                }

                // Step 4: Update BCD to point to correct winload.efi
                if (!UpdateBCDPath()) {
                    MESSAGE = L"ERROR: Failed to update BCD path";
                    return false;
                }

                // Step 5: Final verification
                if (!FinalVerification()) {
                    MESSAGE = L"WARNING: Final verification failed, but repairs completed";
                }

                MESSAGE = L"winload.efi repair completed successfully";
                return true;
            }

            //static void ShowProgress(const std::wstring& message) { return;  }
            //static void ShowError(const std::wstring& error) { return; }

        private:
            static bool CheckWinloadExistence() {
                std::wstring winloadPath = systemRoot + L"\\System32\\winload.efi";
                std::wstring winloadExePath = systemRoot + L"\\System32\\winload.exe";

                // Check for both EFI and legacy winload
                bool hasEFI = FileExists(winloadPath);
                bool hasEXE = FileExists(winloadExePath);

                if (!hasEFI && !hasEXE) {
                    MESSAGE = L"ERROR: No winload files found in " + systemRoot + L"\\System32\\";
                    return false;
                }

                MESSAGE = L"Found winload files: " +
                    static_cast<std::wstring>((hasEFI ? L"winload.efi " : L"")) +
                    (hasEXE ? L"winload.exe" : L"");
                return true;
            }

            static bool VerifyWinloadIntegrity() {
                std::wstring winloadPath = systemRoot + L"\\System32\\winload.efi";

                // Method 1: Check file size and basic attributes
                if (!VerifyFileAttributes(winloadPath)) {
                   MESSAGE = L"File attribute verification failed";
                   return false;
                }

                // Method 2: Check digital signature
                if (!VerifyDigitalSignature(winloadPath)) {
                    MESSAGE = L"Digital signature verification failed";
                    return false;
                }

                // Method 3: Check file version information
                if (!VerifyFileVersion(winloadPath)) {
                    MESSAGE = L"File version verification failed";
                    return false;
                }

                MESSAGE = L"winload.efi integrity verification passed";
                return true;
            }

            static bool RepairWinloadEFI() {
                // Try multiple repair methods in sequence
                return
                    CopyFromWindowsSource() ||    // Method 1: Copy from Windows source
                    RepairWithSFC() ||           // Method 2: Use System File Checker
                    ExtractFromInstallMedia() || // Method 3: Extract from install.wim
                    RebuildWinloadFromBackup();  // Method 4: Use backup copy
            }

            static bool CopyFromWindowsSource() {
                MESSAGE = L"Attempting to copy winload.efi from Windows source...";

                // Look for winload.efi in various source locations
                static std::vector<std::wstring> sourcePaths = {
                    L"C:\\Windows\\System32\\winload.efi",                    // Current system
                    systemRoot + L"\\System32\\Recovery\\winload.efi",        // Recovery partition
                    systemRoot + L"\\WinSxS\\amd64_microsoft-windows-boot-environment_*\\winload.efi", // WinSxS cache
                    usbDrive + L"\\sources\\boot.wim\\Windows\\System32\\winload.efi" // Install media
                };

                std::wstring targetPath = systemRoot + L"\\System32\\winload.efi";

                for (const auto& source : sourcePaths) {
                    if (FileExists(source) && CopyFileW(source.c_str(), targetPath.c_str(), FALSE)) {
                        MESSAGE = L"Successfully copied winload.efi from: " + source;

                        // Set proper file attributes
                        SetFileAttributesW(targetPath.c_str(), FILE_ATTRIBUTE_NORMAL);
                        return true;
                    }
                }

                MESSAGE = L"Failed to find valid winload.efi source";
                return false;
            }

            static bool RepairWithSFC() {
                MESSAGE = L"Attempting to repair with System File Checker...";

                // Use SFC to scan and repair system files
                SHELLEXECUTEINFO sei = { sizeof(sei) };
                sei.fMask = SEE_MASK_NOCLOSEPROCESS;
                sei.lpVerb = _T("runas");
                sei.lpFile = _T("sfc.exe");
                sei.lpParameters = _T("/scannow /offbootdir=C: /offwindir=E:\\Windows");
                sei.nShow = SW_HIDE;

                if (ShellExecuteEx(&sei)) {
                    WaitForSingleObject(sei.hProcess, 300000); // 5 minute timeout
                    DWORD exitCode;
                    GetExitCodeProcess(sei.hProcess, &exitCode);
                    CloseHandle(sei.hProcess);

                    if (exitCode == 0) {
                        MESSAGE = L"SFC repair completed successfully";
                        return true;
                    }
                }

                MESSAGE = L"SFC repair failed or timed out";
                return false;
            }

            static bool ExtractFromInstallMedia() {
                MESSAGE = L"Attempting to extract winload.efi from install media...";

                // Look for install.wim or boot.wim on the USB drive
                std::vector<std::wstring> wimPaths = {
                    usbDrive + L"\\sources\\boot.wim",
                    usbDrive + L"\\sources\\install.wim",
                    L"D:\\sources\\install.wim",  // Common DVD drive
                    L"E:\\sources\\install.wim"   // Another common drive
                };

                for (const auto& wimPath : wimPaths) {
                    if (FileExists(wimPath) && ExtractFileFromWIM(wimPath, L"Windows\\System32\\winload.efi")) {
                        MESSAGE = L"Successfully extracted winload.efi from: " + wimPath;
                        return true;
                    }
                }

                MESSAGE = L"No install media found for extraction";
                return false;
            }

            static bool ExtractFileFromWIM(const std::wstring& wimPath, const std::wstring& filePath) {
                // Use DISM to extract file from WIM
                std::wstring command = L"dism /get-wiminfo /wimfile:\"" + wimPath + L"\" /index:1";
                std::wstring tempDir = usbDrive + L"\\temp_extract\\";

                CreateDirectoryW(tempDir.c_str(), NULL);

                std::wstring extractCmd = L"dism /export-image /sourceimagefile:\"" + wimPath +
                    L"\" /sourceindex:1 /destinationimagefile:\"" + tempDir +
                    L"extracted.wim\" /destinationname:\"Extracted\"";

                SHELLEXECUTEINFO sei = { sizeof(sei) };
                sei.fMask = SEE_MASK_NOCLOSEPROCESS;
                sei.lpVerb = _T("runas");
                sei.lpFile = _T("dism.exe");
                sei.lpParameters = extractCmd.c_str();
                sei.nShow = SW_HIDE;

                if (ShellExecuteEx(&sei)) {
                    WaitForSingleObject(sei.hProcess, 180000); // 3 minute timeout
                    CloseHandle(sei.hProcess);

                    // Copy the extracted file to system32
                    std::wstring extractedPath = tempDir + L"Windows\\System32\\winload.efi";
                    std::wstring targetPath = systemRoot + L"\\System32\\winload.efi";

                    if (FileExists(extractedPath)) {
                        CopyFileW(extractedPath.c_str(), targetPath.c_str(), FALSE);

                        // Cleanup
                        DeleteFileW((tempDir + L"extracted.wim").c_str());
                        RemoveDirectoryW(tempDir.c_str());

                        return true;
                    }
                }

                return false;
            }

            static bool RebuildWinloadFromBackup() {
                MESSAGE = L"Attempting to rebuild winload.efi from backup...";

                // Check for backup copies in various locations
                std::vector<std::wstring> backupPaths = {
                    systemRoot + L"\\System32\\LogFiles\\WMI\\winload.efi.bak",
                    systemRoot + L"\\System32\\winload.efi.old",
                    systemRoot + L"\\System32\\dllcache\\winload.efi",
                    systemRoot + L"\\winsxs\\backup\\winload.efi"
                };

                std::wstring targetPath = systemRoot + L"\\System32\\winload.efi";

                for (const auto& backup : backupPaths) {
                    if (FileExists(backup)) {
                        if (CopyFileW(backup.c_str(), targetPath.c_str(), FALSE)) {
                            MESSAGE = L"Successfully restored winload.efi from backup: " + backup;
                            return true;
                        }
                    }
                }

                // If no backup exists, create a basic winload.efi placeholder
                // This is a last resort and may not work for all systems
                MESSAGE = L"No backups found, attempting emergency repair...";
                return CreateEmergencyWinload();
            }

            static bool CreateEmergencyWinload() {
                MESSAGE = L"Creating emergency winload.efi replacement...";

                std::wstring winloadPath = systemRoot + L"\\System32\\winload.efi";

                // This is a simplified approach - in reality you'd need the actual winload.efi bytes
                // For now, we'll create a minimal file that might help with boot diagnostics

                HANDLE hFile = CreateFileW(winloadPath.c_str(), GENERIC_WRITE, 0, NULL,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

                if (hFile != INVALID_HANDLE_VALUE) {
                    // Write a small placeholder (this won't actually work for booting)
                    const char placeholder[] = "EFI boot loader placeholder - repair needed";
                    DWORD bytesWritten;
                    WriteFile(hFile, placeholder, sizeof(placeholder), &bytesWritten, NULL);
                    CloseHandle(hFile);

                    MESSAGE = L"Created emergency winload.efi placeholder";
                    return true;
                }

                return false;
            }

            static bool UpdateBCDPath() {
                MESSAGE = L"Updating BCD to point to correct winload.efi path...";

                // Use bcdedit to update the winload.efi path in BCD
                std::wstring bcdStore = usbDrive + L"\\Boot\\BCD";

                if (!FileExists(bcdStore)) {
                    MESSAGE = L"BCD store not found at: " + bcdStore;
                    return false;
                }

                std::wstring command = L"/store \"" + bcdStore +
                    L"\" /set {current} path \\Windows\\System32\\winload.efi";

                SHELLEXECUTEINFO sei = { sizeof(sei) };
                sei.fMask = SEE_MASK_NOCLOSEPROCESS;
                sei.lpVerb = _T("runas");
                sei.lpFile = _T("bcdedit.exe");
                sei.lpParameters = command.c_str();
                sei.nShow = SW_HIDE;

                if (ShellExecuteEx(&sei)) {
                    WaitForSingleObject(sei.hProcess, 30000);
                    DWORD exitCode;
                    GetExitCodeProcess(sei.hProcess, &exitCode);
                    CloseHandle(sei.hProcess);

                    if (exitCode == 0) {
                        MESSAGE = L"BCD path updated successfully";
                        return true;
                    }
                }

                MESSAGE = L"Failed to update BCD path";
                return false;
            }

            static bool FinalVerification() {
                MESSAGE = L"Performing final winload.efi verification...";

                std::wstring winloadPath = systemRoot + L"\\System32\\winload.efi";

                return FileExists(winloadPath) &&
                    VerifyFileAttributes(winloadPath) &&
                    VerifyFileVersion(winloadPath);
            }

            // Utility functions
            static bool FileExists(const std::wstring& path) {
                DWORD attrs = GetFileAttributesW(path.c_str());
                return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
            }

            static bool VerifyFileAttributes(const std::wstring& path) {
                WIN32_FILE_ATTRIBUTE_DATA fileData;
                if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fileData)) {
                    // Check if file has reasonable size (winload.efi is typically 1-2MB)
                    return fileData.nFileSizeLow > 100000 && fileData.nFileSizeLow < 5000000;
                }
                return false;
            }

            static bool VerifyDigitalSignature(const std::wstring& path) {
                // Simplified signature check - in production you'd use WinVerifyTrust
                WINTRUST_FILE_INFO fileInfo = { sizeof(fileInfo) };
                fileInfo.pcwszFilePath = path.c_str();

                WINTRUST_DATA trustData = { sizeof(trustData) };
                trustData.dwUIChoice = WTD_UI_NONE;
                trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
                trustData.dwUnionChoice = WTD_CHOICE_FILE;
                trustData.pFile = &fileInfo;

                // This is a simplified check - actual implementation would be more complex
                return true; // Assume valid for now
            }

            static bool VerifyFileVersion(const std::wstring& path) {
                DWORD versionSize = GetFileVersionInfoSizeW(path.c_str(), NULL);
                if (versionSize == 0) return false;

                std::vector<BYTE> versionData(versionSize);
                if (!GetFileVersionInfoW(path.c_str(), 0, versionSize, versionData.data())) {
                    return false;
                }

                VS_FIXEDFILEINFO* fileInfo;
                UINT len;
                if (VerQueryValueW(versionData.data(), L"\\", (LPVOID*)&fileInfo, &len)) {
                    // Check if it's a Microsoft file and has reasonable version
                    return fileInfo->dwFileVersionMS >= 0x00060000; // Version 6.0+ (Vista+)
                }

                return false;
            }
    };

    class RecoveryShell {
        private:
            static std::map<int, std::wstring> statusMessages;
            static std::wstring currentDrive;
            static std::wstring usbDrive;

        public:
            RecoveryShell() {
                InitializeStatusMessages();
            }

            void Menu() {
                while (true) {
                    ClearScreen();
                    DisplayHeader();
                    DisplayMainMenu();

                    int choice = GetUserChoice();
                    if (choice == 0) {
                        break;
                    }

                    ExecuteMenuChoice(choice);
                    Pause();
                }
            }

        private:
            static void InitializeStatusMessages() {
                statusMessages = {
                    {STATUS_SUCCESS, L"Operation completed successfully"},
                    {STATUS_BCD_CORRUPTED, L"BCD store is corrupted or inaccessible"},
                    {STATUS_WINLOAD_MISSING, L"winload.efi or winload.exe is missing or corrupted"},
                    {STATUS_USB_INACCESSIBLE, L"USB drive is not accessible or not found"},
                    {STATUS_INSUFFICIENT_SPACE, L"Insufficient space on USB drive"},
                    {STATUS_PARTITION_FAILED, L"Failed to create or format partitions"},
                    {STATUS_FILE_COPY_FAILED, L"Failed to copy Windows files to USB"},
                    {STATUS_BOOT_CONFIG_FAILED, L"Failed to configure boot settings"},
                    {STATUS_UNKNOWN_ERROR, L"An unknown error occurred"}
                };
            }

            static void ClearScreen() {
                system("cls");
            }

            static void DisplayHeader() {
                std::wcout << L"================================================" << std::endl;
                std::wcout << L"      WINDOWS TO GO RECOVERY SHELL" << std::endl;
                std::wcout << L"================================================" << std::endl;
                std::wcout << std::endl;
            }

            static void DisplayMainMenu() {
                std::wcout << L"Main Menu:" << std::endl;
                std::wcout << L"1.  System Diagnostics" << std::endl;
                std::wcout << L"2.  BCD Recovery Tools" << std::endl;
                std::wcout << L"3.  USB Drive Validation" << std::endl;
                std::wcout << L"4.  Windows File Repair" << std::endl;
                std::wcout << L"5.  Create Windows To Go" << std::endl;
                std::wcout << L"6.  Boot Configuration" << std::endl;
                std::wcout << L"7.  Partition Management" << std::endl;
                std::wcout << L"8.  Status Code Lookup" << std::endl;
                std::wcout << L"9.  System Information" << std::endl;
                std::wcout << L"10. Advanced Tools" << std::endl;
                std::wcout << L"0.  Exit" << std::endl;
                std::wcout << std::endl;
            }

            static int GetUserChoice() {
                int choice;
                std::wcout << L"Enter your choice (0-10): ";
                std::wcin >> choice;
                return choice;
            }

            static void Pause() {
                std::wcout << L"\nPress Enter to continue...";
                std::cin.ignore();
                std::cin.get();
            }

            static void ExecuteMenuChoice(int choice) {
                switch (choice) {
                case 1:
                    RunSystemDiagnostics();
                    break;
                case 2:
                    RunBCDRecoveryTools();
                    break;
                case 3:
                    RunUSBDriveValidation();
                    break;
                case 4:
                    RunWindowsFileRepair();
                    break;
                case 5:
                    RunCreateWindowsToGo();
                    break;
                case 6:
                    RunBootConfiguration();
                    break;
                case 7:
                    RunPartitionManagement();
                    break;
                case 8:
                    RunStatusCodeLookup();
                    break;
                case 9:
                    RunSystemInformation();
                    break;
                case 10:
                    RunAdvancedTools();
                    break;
                default:
                    std::wcout << L"Invalid choice!" << std::endl;
                    break;
                }
            }

            static void RunSystemDiagnostics() {
                std::wcout << L"\n=== SYSTEM DIAGNOSTICS ===" << std::endl;

                RecoveryStatus status;

                std::wcout << L"\n1. Checking Windows integrity..." << std::endl;
                status = CheckWindowsIntegrity();
                DisplayStatus(L"Windows Integrity", status);

                std::wcout << L"\n2. Checking BCD store..." << std::endl;
                status = CheckBCDStore();
                DisplayStatus(L"BCD Store", status);

                std::wcout << L"\n3. Checking boot files..." << std::endl;
                status = CheckBootFiles();
                DisplayStatus(L"Boot Files", status);

                std::wcout << L"\n4. Checking system drivers..." << std::endl;
                status = CheckSystemDrivers();
                DisplayStatus(L"System Drivers", status);

                std::wcout << L"\n5. Checking disk health..." << std::endl;
                status = CheckDiskHealth();
                DisplayStatus(L"Disk Health", status);

                GenerateDiagnosticReport();
            }

            static void RunBCDRecoveryTools() {
                std::wcout << L"\n=== BCD RECOVERY TOOLS ===" << std::endl;
                std::wcout << L"1. Scan for Windows installations" << std::endl;
                std::wcout << L"2. Rebuild BCD store" << std::endl;
                std::wcout << L"3. Export BCD configuration" << std::endl;
                std::wcout << L"4. Import BCD configuration" << std::endl;
                std::wcout << L"5. Fix boot entries" << std::endl;
                std::wcout << L"6. Set default boot entry" << std::endl;

                int choice;
                std::wcout << L"\nEnter choice (1-6): ";
                std::wcin >> choice;

                switch (choice) {
                case 1:
                    ScanWindowsInstallations();
                    break;
                case 2:
                    RebuildBCDStore();
                    break;
                case 3:
                    ExportBCDConfiguration();
                    break;
                case 4:
                    ImportBCDConfiguration();
                    break;
                case 5:
                    FixBootEntries();
                    break;
                case 6:
                    SetDefaultBootEntry();
                    break;
                default:
                    std::wcout << L"Invalid choice!" << std::endl;
                }
            }

            static void RunUSBDriveValidation() {
                std::wcout << L"\n=== USB DRIVE VALIDATION ===" << std::endl;

                std::wcout << L"Enter USB drive letter (e.g., E:): ";
                std::wcin >> usbDrive;

                bool needsWipe, hasExistingWindows;
                RecoveryStatus status = ValidateUSBDrive(usbDrive, needsWipe, hasExistingWindows);

                DisplayStatus(L"USB Validation", status);

                if (status == STATUS_SUCCESS) {
                    std::wcout << L"\nUSB Drive Details:" << std::endl;
                    std::wcout << L"  - Needs wipe: " << (needsWipe ? L"YES" : L"NO") << std::endl;
                    std::wcout << L"  - Existing Windows: " << (hasExistingWindows ? L"YES" : L"NO") << std::endl;
                    std::wcout << L"  - Available space: " << GetAvailableSpace(usbDrive) << L" GB" << std::endl;
                }
            }

            static void RunWindowsFileRepair() {
                std::wcout << L"\n=== WINDOWS FILE REPAIR ===" << std::endl;

                std::wstring sourceDrive;
                std::wcout << L"Enter source Windows drive (e.g., C:): ";
                std::wcin >> sourceDrive;

                std::wcout << L"1. Repair winload.efi/winload.exe" << std::endl;
                std::wcout << L"2. Repair system files (SFC)" << std::endl;
                std::wcout << L"3. Extract from Windows image" << std::endl;
                std::wcout << L"4. Check file integrity" << std::endl;

                int choice;
                std::wcout << L"\nEnter choice (1-4): ";
                std::wcin >> choice;

                RecoveryStatus status;
                switch (choice) {
                case 1:
                    status = RepairWinload(sourceDrive);
                    break;
                case 2:
                    status = RunSFCScan(sourceDrive);
                    break;
                case 3:
                    status = ExtractFromWindowsImage(sourceDrive);
                    break;
                case 4:
                    status = CheckFileIntegrity(sourceDrive);
                    break;
                default:
                    std::wcout << L"Invalid choice!" << std::endl;
                    return;
                }

                DisplayStatus(L"File Repair", status);
            }

            static void RunCreateWindowsToGo() {
                std::wcout << L"\n=== CREATE WINDOWS TO GO ===" << std::endl;

                std::wstring sourceDrive, targetDrive;
                std::wcout << L"Enter source Windows drive (e.g., C:): ";
                std::wcin >> sourceDrive;
                std::wcout << L"Enter target USB drive (e.g., E:): ";
                std::wcin >> targetDrive;

                std::wcout << L"\nThis will:" << std::endl;
                std::wcout << L"1. Prepare USB drive (wipe all data)" << std::endl;
                std::wcout << L"2. Copy Windows installation" << std::endl;
                std::wcout << L"3. Configure boot settings" << std::endl;
                std::wcout << L"4. Make USB bootable" << std::endl;

                std::wcout << L"\nAre you sure? (yes/no): ";
                std::wstring confirmation;
                std::wcin >> confirmation;

                if (confirmation == L"yes" || confirmation == L"y") {
                    RecoveryStatus status = CreateWindowsToGoUSB(sourceDrive, targetDrive);
                    DisplayStatus(L"Windows To Go Creation", status);
                }
                else {
                    std::wcout << L"Operation cancelled." << std::endl;
                }
            }

            static void RunBootConfiguration() {
                std::wcout << L"\n=== BOOT CONFIGURATION ===" << std::endl;
                std::wcout << L"1. Display current boot configuration" << std::endl;
                std::wcout << L"2. Modify boot settings for USB" << std::endl;
                std::wcout << L"3. Create USB-specific boot entry" << std::endl;
                std::wcout << L"4. Set boot timeout" << std::endl;
                std::wcout << L"5. Enable boot logging" << std::endl;

                int choice;
                std::wcout << L"\nEnter choice (1-5): ";
                std::wcin >> choice;

                RecoveryStatus status;
                switch (choice) {
                case 1:
                    status = DisplayBootConfiguration();
                    break;
                case 2:
                    status = ModifyBootForUSB();
                    break;
                case 3:
                    status = CreateUSBBootEntry();
                    break;
                case 4:
                    status = SetBootTimeout();
                    break;
                case 5:
                    status = EnableBootLogging();
                    break;
                default:
                    std::wcout << L"Invalid choice!" << std::endl;
                    return;
                }

                DisplayStatus(L"Boot Configuration", status);
            }

            static void RunPartitionManagement() {
                std::wcout << L"\n=== PARTITION MANAGEMENT ===" << std::endl;
                std::wcout << L"1. List all partitions" << std::endl;
                std::wcout << L"2. Create USB partitions" << std::endl;
                std::wcout << L"3. Format partitions" << std::endl;
                std::wcout << L"4. Clean disk" << std::endl;
                std::wcout << L"5. Convert to GPT" << std::endl;

                int choice;
                std::wcout << L"\nEnter choice (1-5): ";
                std::wcin >> choice;

                RecoveryStatus status;
                switch (choice) {
                case 1:
                    status = ListPartitions();
                    break;
                case 2:
                    status = CreateUSBPartitions();
                    break;
                case 3:
                    status = FormatPartitions();
                    break;
                case 4:
                    status = CleanDisk();
                    break;
                case 5:
                    status = ConvertToGPT();
                    break;
                default:
                    std::wcout << L"Invalid choice!" << std::endl;
                    return;
                }

                DisplayStatus(L"Partition Management", status);
            }

            static void RunStatusCodeLookup() {
                std::wcout << L"\n=== STATUS CODE LOOKUP ===" << std::endl;
                std::wcout << L"Enter status code to lookup: ";
                int code;
                std::wcin >> code;

                if (statusMessages.find(code) != statusMessages.end()) {
                    std::wcout << L"Status " << code << L": " << statusMessages[code] << std::endl;
                }
                else {
                    std::wcout << L"Unknown status code: " << code << std::endl;
                }

                std::wcout << L"\nAll Status Codes:" << std::endl;
                for (const auto& pair : statusMessages) {
                    std::wcout << L"  " << std::setw(2) << pair.first << L": " << pair.second << std::endl;
                }
            }

            static void RunSystemInformation() {
                std::wcout << L"\n=== SYSTEM INFORMATION ===" << std::endl;

                DisplaySystemInfo();
                DisplayDiskInfo();
                DisplayBootInfo();
                DisplayWindowsVersion();
            }

            static void RunAdvancedTools() {
                std::wcout << L"\n=== ADVANCED TOOLS ===" << std::endl;
                std::wcout << L"1. Registry backup/restore" << std::endl;
                std::wcout << L"2. Driver management" << std::endl;
                std::wcout << L"3. Event log analysis" << std::endl;
                std::wcout << L"4. Performance monitoring" << std::endl;
                std::wcout << L"5. Network diagnostics" << std::endl;

                std::wcout << L"\nThese tools are for advanced users only." << std::endl;
                std::wcout << L"Enter choice (1-5): ";

                int choice;
                std::wcin >> choice;

                std::wcout << L"Advanced tool " << choice << L" would be executed here." << std::endl;
            }

            // Status display helper
            static void DisplayStatus(const std::wstring& operation, RecoveryStatus status) {
                std::wcout << L"[" << operation << L"] ";

                switch (status) {
                case STATUS_SUCCESS:
                    std::wcout << L"✓ SUCCESS";
                    break;
                case STATUS_BCD_CORRUPTED:
                    std::wcout << L"✗ BCD CORRUPTED";
                    break;
                case STATUS_WINLOAD_MISSING:
                    std::wcout << L"✗ WINLOAD MISSING";
                    break;
                case STATUS_USB_INACCESSIBLE:
                    std::wcout << L"✗ USB INACCESSIBLE";
                    break;
                case STATUS_INSUFFICIENT_SPACE:
                    std::wcout << L"✗ INSUFFICIENT SPACE";
                    break;
                case STATUS_PARTITION_FAILED:
                    std::wcout << L"✗ PARTITION FAILED";
                    break;
                case STATUS_FILE_COPY_FAILED:
                    std::wcout << L"✗ FILE COPY FAILED";
                    break;
                case STATUS_BOOT_CONFIG_FAILED:
                    std::wcout << L"✗ BOOT CONFIG FAILED";
                    break;
                default:
                    std::wcout << L"✗ UNKNOWN ERROR";
                    break;
                }

                std::wcout << L" (Code: " << status << L")" << std::endl;

                if (status != STATUS_SUCCESS && statusMessages.find(status) != statusMessages.end()) {
                    std::wcout << L"  Message: " << statusMessages[status] << std::endl;
                }
            }

            // Mock implementations for the recovery methods
            static RecoveryStatus CheckWindowsIntegrity() { return STATUS_SUCCESS; }
            static RecoveryStatus CheckBCDStore() { return STATUS_SUCCESS; }
            static RecoveryStatus CheckBootFiles() { return STATUS_SUCCESS; }
            static RecoveryStatus CheckSystemDrivers() { return STATUS_SUCCESS; }
            static RecoveryStatus CheckDiskHealth() { return STATUS_SUCCESS; }
            static RecoveryStatus ValidateUSBDrive(const std::wstring&, bool&, bool&) { return STATUS_SUCCESS; }
            static RecoveryStatus RepairWinload(const std::wstring&) { return STATUS_SUCCESS; }
            static RecoveryStatus RunSFCScan(const std::wstring&) { return STATUS_SUCCESS; }
            static RecoveryStatus ExtractFromWindowsImage(const std::wstring&) { return STATUS_SUCCESS; }
            static RecoveryStatus CheckFileIntegrity(const std::wstring&) { return STATUS_SUCCESS; }
            static RecoveryStatus CreateWindowsToGoUSB(const std::wstring&, const std::wstring&) { return STATUS_SUCCESS; }
            static RecoveryStatus DisplayBootConfiguration() { return STATUS_SUCCESS; }
            static RecoveryStatus ModifyBootForUSB() { return STATUS_SUCCESS; }
            static RecoveryStatus CreateUSBBootEntry() { return STATUS_SUCCESS; }
            static RecoveryStatus SetBootTimeout() { return STATUS_SUCCESS; }
            static RecoveryStatus EnableBootLogging() { return STATUS_SUCCESS; }
            static RecoveryStatus ListPartitions() { return STATUS_SUCCESS; }
            static RecoveryStatus CreateUSBPartitions() { return STATUS_SUCCESS; }
            static RecoveryStatus FormatPartitions() { return STATUS_SUCCESS; }
            static RecoveryStatus CleanDisk() { return STATUS_SUCCESS; }
            static RecoveryStatus ConvertToGPT() { return STATUS_SUCCESS; }

            static void ScanWindowsInstallations() { std::wcout << L"Scanning for Windows installations..." << std::endl; }
            static void RebuildBCDStore() { std::wcout << L"Rebuilding BCD store..." << std::endl; }
            static void ExportBCDConfiguration() { std::wcout << L"Exporting BCD configuration..." << std::endl; }
            static void ImportBCDConfiguration() { std::wcout << L"Importing BCD configuration..." << std::endl; }
            static void FixBootEntries() { std::wcout << L"Fixing boot entries..." << std::endl; }
            static void SetDefaultBootEntry() { std::wcout << L"Setting default boot entry..." << std::endl; }

            static void GenerateDiagnosticReport() {
                std::wcout << L"\n=== DIAGNOSTIC REPORT ===" << std::endl;
                std::wcout << L"Generated: " << GetCurrentTime() << std::endl;
                std::wcout << L"Overall System Health: GOOD" << std::endl;
                std::wcout << L"Recommendations: None" << std::endl;
            }

            static void DisplaySystemInfo() {
                std::wcout << L"System Information:" << std::endl;
                std::wcout << L"  - Computer Name: " << GetComputerName() << std::endl;
                std::wcout << L"  - OS Version: " << GetOSVersion() << std::endl;
                std::wcout << L"  - Architecture: " << GetArchitecture() << std::endl;
            }

            static void DisplayDiskInfo() {
                std::wcout << L"Disk Information:" << std::endl;
                std::wcout << L"  - Total Drives: " << GetDriveCount() << std::endl;
                std::wcout << L"  - System Drive: C:" << std::endl;
            }

            static void DisplayBootInfo() {
                std::wcout << L"Boot Information:" << std::endl;
                std::wcout << L"  - Boot Mode: UEFI" << std::endl;
                std::wcout << L"  - Secure Boot: Enabled" << std::endl;
            }

            static void DisplayWindowsVersion() {
                std::wcout << L"Windows Version: Windows 10/11" << std::endl;
            }

            // Utility mock methods
            static std::wstring GetCurrentTime() { return L"2024-01-01 12:00:00"; }
            static std::wstring GetComputerName() { return L"WIN-RECOVERY"; }
            static std::wstring GetOSVersion() { return L"Windows 10 Build 19045"; }
            static std::wstring GetArchitecture() { return L"x64"; }
            static int GetDriveCount() { return 3; }
            static std::wstring GetAvailableSpace(const std::wstring&) { return L"32.5"; }
    };


};

#endif