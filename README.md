# Can only be compiled and tested on a windows machine and not a linux machine as for now






# Once there is support, it should look something like this
sudo apt install mingw-w64

# Compile for Windows 64-bit from WSL
x86_64-w64-mingw32-g++ -o WindowsToGoCreator.exe main.cpp \
    -lsetupapi -lwinioctl -static

# Compile for Windows 32-bit
i686-w64-mingw32-g++ -o WindowsToGoCreator.exe main.cpp \
    -lsetupapi -lwinioctl -static