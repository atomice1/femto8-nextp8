#!/bin/bash
# Cross-compile femto8 for Windows using a bash script
# This script supports multiple approaches:
# 1. MSYS2 environment (recommended for Windows development)
# 2. Linux cross-compilation with mingw-w64

set -ex  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}Starting Windows cross-compilation of femto8...${NC}"

# Save original directory to return later
PROJECT_ROOT="$(pwd)"

# Function to check if a command exists
command_exists() {
    command -v "$1" &> /dev/null
}

# Detect the environment
UNAME_S=$(uname -s)

if [[ "$UNAME_S" == *"MINGW"* ]] || [[ "$UNAME_S" == *"MSYS"* ]]; then
    echo -e "${GREEN}Detected MSYS2/MinGW environment${NC}"
    
    # In MSYS2, we can use the native compilers
    if command_exists gcc; then
        CC=gcc
        CXX=g++
        echo -e "${GREEN}Found compiler: $CC${NC}"
    else
        echo -e "${RED}Error: gcc not found in MSYS2 environment.${NC}"
        exit 1
    fi
    
    # In MSYS2, pkg-config should work directly
    if command_exists pkg-config; then
        PKG_CONFIG=pkg-config
    else
        echo -e "${RED}Error: pkg-config not found. Please install pkgconf in MSYS2.${NC}"
        exit 1
    fi
    
else
    # Linux cross-compilation
    echo -e "${YELLOW}Detected Linux system: $UNAME_S${NC}"

    # Detect mingw-w64 toolchain
    echo -e "${YELLOW}Checking for mingw-w64 toolchain...${NC}"

    CC=""
    CXX=""

    # Try different mingw-w64 compiler names
    if command_exists x86_64-w64-mingw32-gcc-posix; then
        CC=x86_64-w64-mingw32-gcc-posix
        CXX=x86_64-w64-mingw32-g++-posix
    elif command_exists x86_64-w64-mingw32-gcc; then
        CC=x86_64-w64-mingw32-gcc
        CXX=x86_64-w64-mingw32-g++
    elif command_exists i686-w64-mingw32-gcc-posix; then
        CC=i686-w64-mingw32-gcc-posix
        CXX=i686-w64-mingw32-g++-posix
    elif command_exists i686-w64-mingw32-gcc; then
        CC=i686-w64-mingw32-gcc
        CXX=i686-w64-mingw32-g++
    else
        echo -e "${RED}Error: mingw-w64 toolchain not found. Please install it first.${NC}"
        echo "On Ubuntu/Debian: sudo apt install gcc-mingw-w64 g++-mingw-w64"
        exit 1
    fi

    echo -e "${GREEN}Found mingw-w64 compiler: $CC${NC}"

    # Don't use pkg-config for cross-compilation - Ubuntu's mingw doesn't include it
    # and system pkg-config would find Linux SDL2. Instead, we'll set flags directly.
    PKG_CONFIG=""
fi

# Check for SDL2 development files for Windows
echo -e "${YELLOW}Checking for SDL2 development files...${NC}"

if [[ "$UNAME_S" == *"MINGW"* ]] || [[ "$UNAME_S" == *"MSYS"* ]]; then
    # MSYS2: use pkg-config as normal
    if command_exists pkg-config; then
        PKG_CONFIG=pkg-config
    else
        echo -e "${RED}Error: pkg-config not found. Please install pkgconf in MSYS2.${NC}"
        exit 1
    fi
fi

if [ -n "$PKG_CONFIG" ]; then
    SDL_CFLAGS_RAW=$($PKG_CONFIG --cflags sdl2 2>/dev/null || echo "")
else
    # Cross-compilation from Linux without mingw pkg-config: we need to build SDL2 from source
    # Start with empty flags; the build process will populate them after building SDL2
    SDL_CFLAGS_RAW=""
fi

if [ -z "$SDL_CFLAGS_RAW" ]; then
    echo -e "${RED}Error: SDL2 development files not found for Windows.${NC}"

    if [[ "$UNAME_S" == *"MINGW"* ]] || [[ "$UNAME_S" == *"MSYS"* ]]; then
        echo "Please install SDL2 in MSYS2:"
        echo "For UCRT64: pacman -S mingw-w64-ucrt-x86_64-SDL2"
        echo "For CLANG64: pacman -S mingw-w64-clang-x86_64-SDL2"
    else
        # Linux - provide instructions for building SDL2 and OpenSSL from source
        echo -e "${BLUE}Building SDL2 and OpenSSL from source for Windows cross-compilation...${NC}"

        SDL2_VERSION="2.30.9"
        OPENSSL_VERSION="3.3.1"
        SDL2_DIR="sdl2-mingw-build"

        # Compute install prefix before changing directories
        TARGET_PREFIX="$(cd "$SDL2_DIR" && pwd)/build"

        mkdir -p "$SDL2_DIR"

        # ====================
        # Build SDL2 if needed
        # ====================
        echo -e "${YELLOW}Checking SDL2 build status...${NC}"

        if [ ! -f "$TARGET_PREFIX/lib/pkgconfig/sdl2.pc" ]; then
            echo -e "${GREEN}SDL2 not found, building from source...${NC}"
            
            # Clean up any previous failed SDL2 builds only
            rm -rf "$SDL2_DIR/SDL2-$SDL2_VERSION"
            rm -f "$SDL2_DIR/SDL2-$SDL2_VERSION.tar.gz"

            cd "$SDL2_DIR"

            # Download SDL2 source
            echo -e "${YELLOW}Downloading SDL2 $SDL2_VERSION source...${NC}"
            wget "https://www.libsdl.org/release/SDL2-$SDL2_VERSION.tar.gz" || {
                echo -e "${RED}Failed to download SDL2. Please install wget or download manually.${NC}"
                exit 1
            }

            tar -xzf "SDL2-$SDL2_VERSION.tar.gz"
            cd "SDL2-$SDL2_VERSION"

            # Configure for Windows cross-compilation
            echo -e "${YELLOW}Configuring SDL2 for Windows cross-compilation...${NC}"

            if [ "$CC" == *"i686"* ]; then
                HOST=i686-w64-mingw32
            else
                HOST=x86_64-w64-mingw32
            fi

            ./configure --host=$HOST --prefix="$TARGET_PREFIX" --enable-shared --disable-static || {
                echo -e "${RED}Failed to configure SDL2. Please check the error messages above.${NC}"
                exit 1
            }

            # Build SDL2
            echo -e "${YELLOW}Building SDL2...${NC}"
            make -j$(nproc) || {
                echo -e "${RED}Failed to build SDL2. Please check the error messages above.${NC}"
                exit 1
            }

            # Install SDL2 to a local directory (prefix already set in configure)
            echo -e "${YELLOW}Installing SDL2 locally...${NC}"
            make install || {
                echo -e "${RED}Failed to install SDL2. Please check the error messages above.${NC}"
                exit 1
            }

            cd ..

            echo -e "${GREEN}SDL2 built and installed successfully!${NC}"
        else
            echo -e "${GREEN}Found existing SDL2 build, skipping rebuild...${NC}"
        fi

        # ====================
        # Build OpenSSL if needed
        # ====================
        echo -e "${YELLOW}Checking OpenSSL build status...${NC}"

        if [ ! -f "$TARGET_PREFIX/lib/libssl.a" ] && [ ! -f "$TARGET_PREFIX/lib64/libssl.a" ]; then
            echo -e "${GREEN}OpenSSL not found, building from source...${NC}"
            
            cd "$SDL2_DIR"

            # Download OpenSSL source
            if [ ! -f "openssl-$OPENSSL_VERSION.tar.gz" ]; then
                echo -e "${YELLOW}Downloading OpenSSL $OPENSSL_VERSION source...${NC}"
                wget "https://www.openssl.org/source/openssl-$OPENSSL_VERSION.tar.gz" || {
                    echo -e "${RED}Failed to download OpenSSL. Please install wget or download manually.${NC}"
                    exit 1
                }
            fi

            tar -xzf "openssl-$OPENSSL_VERSION.tar.gz"

            cd "openssl-$OPENSSL_VERSION"

            # Configure OpenSSL for cross-compilation to Windows
            echo -e "${YELLOW}Configuring OpenSSL for Windows cross-compilation...${NC}"

            if [ "$CC" == *"i686"* ]; then
                CROSS_COMPILE=i686-w64-mingw32-
                OPENSSL_TARGET="mingw"
            else
                CROSS_COMPILE=x86_64-w64-mingw32-
                OPENSSL_TARGET="mingw64"
            fi

            # OpenSSL's Configure prepends CROSS_COMPILE to CC, so temporarily override CC for configure
            # We use a subshell or local variable to avoid affecting the global CC used by make later
            (export CROSS_COMPILE; export CC="gcc"; ./Configure $OPENSSL_TARGET --prefix="$TARGET_PREFIX" no-shared no-dso no-tests -DWIN32 -D_WIN32 -DWIN32_LEAN_AND_MEAN) || {
                echo -e "${RED}Failed to configure OpenSSL. Please check the error messages above.${NC}"
                exit 1
            }

            # Build OpenSSL
            echo -e "${YELLOW}Building OpenSSL...${NC}"
            make -j$(nproc) || {
                echo -e "${RED}Failed to build OpenSSL. Please check the error messages above.${NC}"
                exit 1
            }

            # Install OpenSSL to the same local directory as SDL2
            echo -e "${YELLOW}Installing OpenSSL locally...${NC}"
            make install || {
                echo -e "${RED}Failed to install OpenSSL. Please check the error messages above.${NC}"
                exit 1
            }

            cd ..

            echo -e "${GREEN}OpenSSL built and installed successfully!${NC}"
        else
            echo -e "${GREEN}Found existing OpenSSL build, skipping rebuild...${NC}"
        fi

        # Update SDL_CFLAGS_RAW to point to the built headers if available
        if [ -d "$TARGET_PREFIX/include/SDL2" ]; then
            export SDL_CFLAGS_RAW="-I$TARGET_PREFIX/include/SDL2 -D_GNU_SOURCE=1"
        fi

        # Set up OpenSSL flags (always needed when building from source)
        export OPENSSL_CFLAGS="-I$TARGET_PREFIX/include"
        export OPENSSL_LIBS="-L$TARGET_PREFIX/lib -L$TARGET_PREFIX/lib64 -lssl -lcrypto -lws2_32 -lgdi32"
    fi
fi

# Set up the environment variables and run make
echo -e "${YELLOW}Building femto8 for Windows...${NC}"

# Return to project root before running make (SDL build may have changed directory)
cd "$PROJECT_ROOT"

mkdir -p build-windows

# Run make with cross-compilation settings
make \
    CC="$CC" \
    CXX="$CXX" \
    PLATFORM=windows \
    SDL_CFLAGS_RAW="-I$TARGET_PREFIX/include/SDL2 -D_GNU_SOURCE=1" \
    INCFLAGS="-Isrc -Isrc/data -Isrc/lua -Isrc/lodepng -Isrc/lexaloffle $OPENSSL_CFLAGS" \
    SDL_LIBS_RAW="-L$TARGET_PREFIX/lib -lSDL2 -lpsapi -lws2_32 -lwinmm" \
    OPENSSL_LIBS="-L$TARGET_PREFIX/lib -L$TARGET_PREFIX/lib64 -lssl -lcrypto -lws2_32 -lgdi32 -lcrypt32 -ladvapi32 -luserenv -lncrypt"

# Check if the build was successful
if [ -f "build-windows/femto8.exe" ]; then
    echo -e "${GREEN}Build successful!${NC}"
    echo -e "${YELLOW}Windows executable created: build-windows/femto8.exe${NC}"
    ls -lh "build-windows/femto8.exe"
else
    echo -e "${RED}Build failed or executable not found.${NC}"
    exit 1
fi

echo -e "${GREEN}Cross-compilation completed successfully!${NC}"
