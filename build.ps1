$ARM_PATH = "C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.3 rel1\bin"
$NINJA_PATH = "C:\Program Files (x86)\ninja"
$CMAKE_PATH = "C:\Program Files\CMake\binn"

$env:PATH = "$ARM_PATH;$NINJA_PATH;$CMAKE_PATH;" + $env:PATH
$env:PICO_SDK_PATH = "F:\git\electronics\modular-midi\libs\pico-sdk"

# This flag tells the SDK: "Don't try to sign the binary, I'll handle it."
$BUILD_DIR = "F:\git\electronics\modular-midi\build"
if (!(Test-Path $BUILD_DIR)) { New-Item -ItemType Directory -Path $BUILD_DIR }

cd $BUILD_DIR

# Configure without the 'extra outputs' that trigger the picotool build
cmake -G "Ninja" -DPICO_BOARD=pico2 -DPICO_NO_PICOTOOL=1 -DPICO_PLATFORM=rp2350..

# Build with your 24 threads
ninja -j 24

cd ..