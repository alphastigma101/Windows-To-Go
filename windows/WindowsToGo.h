#ifndef _WINDOWS_TO_GO_H_
#define _WINDOWS_TO_GO_H_
#include "editor/bcd.h"

//#include <wimlib.h>

#include <setupapi.h>
#include <winioctl.h>

//#pragma comment(lib, "wimlib.lib")
//#pragma comment(lib, "bcd.lib")


class WindowsToGoCreator {
    // This is the cli ui and handles the usb formatting
          
    public:

        explicit WindowsToGoCreator(const std::wstring& drive, const std::wstring& windows_drive) 
        : usb_drive(drive), windows(windows_drive)  {
            
            // Validate the bcd and if it is corrupted, we will repair it 
            ShowProgress(MESSAGE);
            if (BCD::ValidateSystemBCD()) {
                // Join the thread
                try {
                    // Every method here will be throw exceptions when needed
                    // We need to create threads for each method called show_progress
                    ShowProgress(MESSAGE);
                    ValidateUSB(); // Check the health of the usb
                    // join the thread here 
                    ShowProgress(MESSAGE); 
                    if (PrepareUSB()) {
                        // Join the thread here
                        ShowProgress(MESSAGE);
                        BCD bcd(drive, windows);
                        bcd.ModifyBootManager();
                    }
                }
                catch(...) {
                    ShowError(ERROR);
                }
            }
             
        }

        ~WindowsToGoCreator() = default;
        
        static void OptimizeWindows();
        static void ValidateWindows();
        static bool ConfirmUserIntent();
 
    private:

        std::wstring usb_drive;
        std::wstring windows;

        static bool ValidateUSB() {
            // Here we validate the usb flash drive's health and wipe out any existing data here
            // Make sure there is enough space to copy over the system to the usb flash drive

            return false;
        }
        static bool PrepareUSB() {
            // We need to convert the driver into windows driver format 
            // We intergrate the api methods into here: https://learn.microsoft.com/en-us/windows/win32/api/winioctl/
            // We find the windows operating system path and we find out the partitions 
            // We then create the partitions onto the usb flash drive with the boot flags and everything.
            // We then copy all the data from the windows operating system to the usb     
            return true;
        }

        static void ShowProgress(const std::wstring& message);
        static void ShowError(const std::wstring& error);

};

#endif