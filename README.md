# Windows To Go Creator

A cross-platform tool for creating Windows To Go drives from Linux and Windows systems.

## Features

- Create Windows To Go drives from Windows ISO files
- Cross-platform compilation (Linux → Windows)
- Secure and validated operations
- Support for both BIOS and UEFI systems

## Quick Start

### Windows (Native)
```cmd
# Clone the repository
git clone https://github.com/yourusername/Windows-To-Go.git
cd windows-to-go-creator

# Build using Visual Studio or CMake
cmake -B build
cmake --build build --config Release

# Run the application
build\Release\WindowsToGoCreator.exe
```

### Linux (Cross-Compilation to Windows)

#### Prerequisites
```bash
# Install MinGW-w64 for cross-compilation
sudo apt update
sudo apt install mingw-w64

# Or on other distributions:
# Fedora: sudo dnf install mingw64-gcc-c++
# Arch: sudo pacman -S mingw-w64-gcc
```

#### Compilation
```bash
# Clone the repository
git clone https://github.com/yourusername/windows-to-go-creator.git
cd windows-to-go-creator

# Cross-compile for Windows 64-bit
x86_64-w64-mingw32-g++ -o WindowsToGoCreator.exe main.cpp \
    -lsetupapi -lwinioctl -static -O2 -DNDEBUG

# Cross-compile for Windows 32-bit
i686-w64-mingw32-g++ -o WindowsToGoCreator.exe main.cpp \
    -lsetupapi -lwinioctl -static -O2 -DNDEBUG

# Using CMake (recommended)
cmake -B build -DCROSS_COMPILE_WINDOWS=ON .
cmake --build build --config Release
```

#### Using WSL (Windows Subsystem for Linux)
```bash
# From WSL, install MinGW and compile:
sudo apt install mingw-w64
x86_64-w64-mingw32-g++ -o WindowsToGoCreator.exe main.cpp \
    -lsetupapi -lwinioctl -static -O2 -DNDEBUG
```

## Advanced Build Options

### Using CMake (Cross-Platform)

```bash
# Debug build with sanitizers
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Release build with optimizations
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Cross-compile from Linux to Windows
cmake -B build -DCROSS_COMPILE_WINDOWS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Build Flags

The project uses several compiler flags for security and performance:

**Release Build Flags:**
- `-O2` / `-O3`: Optimization levels
- `-DNDEBUG`: Disable debug asserts
- `-fstack-protector-strong`: Stack protection
- `-fPIE`: Position Independent Executable
- `-flto`: Link Time Optimization

**Security Hardening:**
- `-pie`: Position Independent Executable
- `-Wl,-z,now`: Immediate binding
- `-Wl,-z,relro`: Read-only after relocation

## Project Structure

```
windows-to-go-creator/
├── CMakeLists.txt          # Cross-platform build configuration
├── main.cpp               # Main application source
├── src/                   # Additional source files
├── include/               # Header files
└── README.md             # This file
```

## Dependencies

### Windows
- Windows SDK (included with Visual Studio)
- setupapi.lib
- winioctl.lib

### Linux (Cross-Compilation)
- mingw-w64 (x86_64-w64-mingw32-g++ or i686-w64-mingw32-g++)
- Standard C++ library

## Usage

1. **Prepare a Windows ISO file**
2. **Connect your USB drive** (minimum 32GB recommended)
3. **Run the application** and follow the prompts
4. **Select your ISO and target drive**
5. **Wait for the process to complete**

## Security Notes

- Always verify Windows ISO integrity
- Backup important data before proceeding
- Use trusted USB drives from reputable manufacturers

## Troubleshooting

### Common Issues

**"Permission denied" on Linux:**
```bash
sudo chmod +x WindowsToGoCreator.exe
```

**Missing dependencies:**
```bash
# Ensure MinGW is properly installed
sudo apt install --reinstall mingw-w64
```

**CMake configuration errors:**
```bash
# Clear build directory and reconfigure
rm -rf build
cmake -B build .
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test both Windows and Linux compilation
5. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Support

For issues and questions:
1. Check the troubleshooting section above
2. Search existing GitHub issues
3. Create a new issue with detailed information

## Notes:
# Should only use ALIAS keyword if you have unit testing executables that require libraries 
# A primary use-case for INTERFACE libraries is header-only libraries
# When we specify the FILE_SET here, the BASE_DIRS we define automatically become include directories in the usage requirements for the target
# Imported executables are useful for convenient reference from commands like add_custom_command().
# An IMPORTED target represents a pre-existing dependency. Usually such targets are defined by an upstream package and should be treated as immutable
# The scope of the definition of an IMPORTED target is the directory where it was defined. It may be accessed and used from subdirectories, but not from parent directories or sibling directories. The scope is similar to the scope of a cmake variable.
# add_executable() and add_library() allow this: $<...> where you can replace it with the stuff here: https://cmake.org/cmake/help/latest/manual/cmake-generator-expressions.7.html#manual:cmake-generator-expressions(7)
https://learn.microsoft.com/en-us/cpp/build/reference/std-specify-language-standard-version?view=msvc-170


# TODOS:
# set_property() could be useful when reconfiguring the interpreter https://cmake.org/cmake/help/latest/command/set_property.html#command:set_property
