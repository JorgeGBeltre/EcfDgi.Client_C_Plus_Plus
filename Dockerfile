# ---------------------------------------------------------------------------
# Multi-stage build for the C++ EcfDgii.Client REST host.
# ---------------------------------------------------------------------------
FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    git curl zip unzip tar ca-certificates \
    build-essential cmake ninja-build pkg-config \
    autoconf automake libtool autoconf-archive python3 \
    bison flex \
 && rm -rf /var/lib/apt/lists/*

# vcpkg
ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone https://github.com/microsoft/vcpkg "$VCPKG_ROOT" \
 && "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics

WORKDIR /src

# Copy vcpkg manifest and custom triplets first to leverage Docker layer caching
COPY vcpkg.json ./
COPY triplets/ ./triplets/

# Pre-install dependencies using a BuildKit cache mount for the vcpkg binary cache
RUN --mount=type=cache,target=/root/.cache/vcpkg \
    $VCPKG_ROOT/vcpkg install --x-install-root=build/vcpkg_installed

# Copy the rest of the application source code
COPY . .

# Configure and build the target executable
RUN cmake --preset default && cmake --build build --target ecfdgii_api

# ---------------------------------------------------------------------------
FROM ubuntu:24.04 AS final

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates libpq5 \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /src/build/ecfdgii_api /app/ecfdgii_api
COPY --from=build /src/config/appsettings.json /app/appsettings.json
COPY --from=build /src/db/schema.sql /app/db/schema.sql

EXPOSE 8080
ENTRYPOINT ["/app/ecfdgii_api"]
