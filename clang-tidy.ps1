# Thin wrapper around tools/clang-tidy-dir.py -- runs clang-tidy on a single
# file or directory, picking the right build tree and ARM cross-compile
# flags automatically. See that script's docstring for full details.
#
# Usage:
#   ./clang-tidy.ps1 src/projects/hub-master/main.cpp
#   ./clang-tidy.ps1 src/shared/hal/rp2350
#   ./clang-tidy.ps1 tests/shared -- -checks=-modernize-use-trailing-return-type
#
# Requires build/ and build-tests/ to already be configured (build.ps1 /
# test.ps1 first), and clang-tidy on PATH or installed at the default LLVM
# location (C:\Program Files\LLVM\bin\clang-tidy.exe).

python3 tools/clang-tidy-dir.py @args
exit $LASTEXITCODE
