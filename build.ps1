$ARM_PATH = "C:\Program Files (x86)\arm-gnu-toolchain\bin"
$NINJA_PATH = "C:\Program Files (x86)\ninja"
$CMAKE_PATH = "C:\Program Files (x86)\cmake-4.3.0-rc2-windows-x86_64\bin"

$env:PATH = "$ARM_PATH;$NINJA_PATH;$CMAKE_PATH;" + $env:PATH
$env:PICO_SDK_PATH = "C:\projects\modular_midi\libs\pico-sdk"

# This flag tells the SDK: "Don't try to sign the binary, I'll handle it."
$BUILD_DIR = "C:\projects\modular_midi\build"
if (!(Test-Path $BUILD_DIR)) { New-Item -ItemType Directory -Path $BUILD_DIR }

cd $BUILD_DIR

# Configure without the 'extra outputs' that trigger the picotool build
cmake -G "Ninja" -DPICO_BOARD=pico2 -DPICO_NO_PICOTOOL=1 ..

# Build with your 24 threads
ninja -j 24