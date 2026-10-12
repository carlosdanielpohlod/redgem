# Builds RedGem and its tests. tools/docker/editor.sh opens the editor; `docker compose run --rm test`
# runs the tests; `docker compose up relay` runs redgem-relay.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
# Compiler and build tools; Ruby and rake build mruby; X11 and OpenGL headers for the window;
# Mesa and Xvfb so the tests can open a window without a display or a graphics card.
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build git ca-certificates xz-utils ruby rake bison \
      libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
      libxext-dev libxfixes-dev libxrender-dev libasound2-dev \
      libgl1-mesa-dri xvfb xauth \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /redgem
COPY . .
RUN cmake -S . -B build -G Ninja && cmake --build build

# Inside a container there is one graphics device: whatever the host hands over (or Mesa's software one).
ENV REDGEM_GPU=integrated
CMD ["build/src/redgem"]
