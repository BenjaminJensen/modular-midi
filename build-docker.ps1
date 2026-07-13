# Container counterpart to build.ps1 — same firmware build, run inside the
# Docker image defined in docker/Dockerfile instead of the native Windows
# ARM toolchain.
#
# Output goes to build-docker/, not build/: CMakeCache.txt bakes in absolute
# compiler paths and a target ABI, so a Windows-native configure and a
# Linux-container configure can't safely share one build directory.

$IMAGE = "modular-midi-build"

docker image inspect $IMAGE *> $null
if ($LASTEXITCODE -ne 0) {
    docker build -t $IMAGE -f docker/Dockerfile docker/
}

docker run --rm -v "${PWD}:/workspace" -w /workspace $IMAGE `
    bash -c "cmake -G Ninja -B build-docker -DPICO_BOARD=pico2 -DPICO_NO_PICOTOOL=1 -DPICO_PLATFORM=rp2350 -DCMAKE_BUILD_TYPE=Debug && cmake --build build-docker"
