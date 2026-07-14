$MSYS2_PATH = "C:\msys64\ucrt64\bin"

# MSYS2 UCRT64 g++ must come before anything else on PATH, otherwise
# arm-none-eabi-gcc or MSVC's cl (also on PATH on this machine) can get
# picked up instead - see CLAUDE.md's Testing section for why the host
# tests need a native compiler, not the ARM cross-compiler.
$env:PATH = "$MSYS2_PATH;" + $env:PATH

$REPO_DIR = "F:\git\electronics\modular-midi"
$BUILD_DIR = "$REPO_DIR\build-tests"

cd $REPO_DIR

cmake -G "Ninja" -S tests -B $BUILD_DIR -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build $BUILD_DIR
ctest --test-dir $BUILD_DIR --output-on-failure
