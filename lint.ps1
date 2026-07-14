# Runs the same diff-based clang-tidy check CI runs (tools/lint-diff.sh),
# inside the modular-midi-build Docker image, diffed against origin/main.
#
# Must be run from PowerShell, not Git Bash: Git Bash's path translation
# mangles the `-v "${PWD}:/workspace"` mount spec (rewrites it to something
# like "C:/Program Files/Git/workspace"), so `docker run` fails outright
# from that shell. Requires Docker Desktop running.
#
# Uses build-docker/ and build-tests-docker/, not build/ and build-tests/,
# so this never disturbs your native Windows build trees (see build-docker.ps1).

$IMAGE = "modular-midi-build"

docker image inspect $IMAGE *> $null
if ($LASTEXITCODE -ne 0) {
    docker build -t $IMAGE -f docker/Dockerfile docker/
}

# Keep origin/main current so the diff isn't stale.
git fetch origin main:refs/remotes/origin/main

# lint-diff.sh needs compile_commands.json from both trees.
docker run --rm -v "${PWD}:/workspace" -w /workspace $IMAGE `
    bash -c "cmake -G Ninja -B build-docker -DPICO_BOARD=pico2 -DPICO_NO_PICOTOOL=1 -DPICO_PLATFORM=rp2350 -DCMAKE_BUILD_TYPE=Debug && cmake --build build-docker"

docker run --rm -v "${PWD}:/workspace" -w /workspace $IMAGE `
    bash -c "cmake -G Ninja -S tests -B build-tests-docker -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build-tests-docker"

docker run --rm -v "${PWD}:/workspace" -w /workspace $IMAGE `
    bash tools/lint-diff.sh origin/main build-docker build-tests-docker
