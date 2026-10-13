# Multi-stage build: the first stage compiles the server with all the tools,
# and the second stage copies only the finished program into a small image.

# ---- Stage 1: build ---------------------------------------------------------
FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive
# Package versions are not pinned: Ubuntu security updates would break a pinned build.
# hadolint ignore=DL3008
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git curl zip unzip tar pkg-config \
        autoconf automake libtool ca-certificates jq \
    && rm -rf /var/lib/apt/lists/*

# Clone vcpkg at the same baseline as vcpkg.json. The clone is done before the
# project files are copied, so Docker can reuse this layer when only code changes.
ENV VCPKG_ROOT=/opt/vcpkg
WORKDIR /src
COPY vcpkg.json ./
RUN git clone https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT" \
    && git -C "$VCPKG_ROOT" checkout "$(jq -r '."builtin-baseline"' vcpkg.json)" \
    && "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics

# Install the libraries as their own layer. It is rebuilt only when vcpkg.json
# changes, which saves a long Boost build on every code change.
RUN "$VCPKG_ROOT/vcpkg" install --x-manifest-root=/src --x-install-root=/src/vcpkg_installed

# Now copy the source code and build the release preset.
COPY . .
RUN cmake --preset release && cmake --build --preset release

# ---- Stage 2: runtime -------------------------------------------------------
FROM ubuntu:24.04 AS runtime

# hadolint ignore=DL3008
RUN apt-get update && apt-get install -y --no-install-recommends libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# Run as a non-root user, so a bug in the server cannot change system files.
RUN useradd --system --no-create-home --shell /usr/sbin/nologin matchforge

COPY --from=build /src/build/release/src/server/matchforge_server /usr/local/bin/matchforge_server

USER matchforge
ENTRYPOINT ["/usr/local/bin/matchforge_server"]
