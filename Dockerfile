# ---------------------------------------------------------------------------
# Multi-stage build for the C++ EcfDgii.Client REST host.
# ---------------------------------------------------------------------------

FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive
ENV VCPKG_ROOT=/opt/vcpkg
ENV VCPKG_DEFAULT_BINARY_CACHE=/vcpkg-bincache

RUN apt-get update && apt-get install -y --no-install-recommends \
    git curl zip unzip tar ca-certificates \
    build-essential cmake ninja-build pkg-config \
    autoconf automake libtool autoconf-archive python3 \
    bison flex \
 && rm -rf /var/lib/apt/lists/*

# vcpkg
RUN git clone https://github.com/microsoft/vcpkg "$VCPKG_ROOT" \
 && "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics \
 && mkdir -p /vcpkg-bincache /opt/vcpkg/downloads

WORKDIR /src

# Copy vcpkg manifest and custom triplets first to leverage Docker layer caching
COPY vcpkg.json ./
COPY triplets/ ./triplets/

# Install dependencies using persistent BuildKit cache
# /vcpkg-bincache         -> compiled binary packages
# /opt/vcpkg/downloads   -> downloaded source archives
RUN --mount=type=cache,id=ecfdgii-vcpkg-bincache,target=/vcpkg-bincache \
    --mount=type=cache,id=ecfdgii-vcpkg-downloads,target=/opt/vcpkg/downloads \
    "$VCPKG_ROOT/vcpkg" install --x-install-root=/src/build/vcpkg_installed

# Copy the rest of the application source code
COPY . .

# Configure and build the target executable
RUN --mount=type=cache,id=ecfdgii-vcpkg-bincache,target=/vcpkg-bincache \
    --mount=type=cache,id=ecfdgii-vcpkg-downloads,target=/opt/vcpkg/downloads \
    cmake --preset default \
 && cmake --build build --target ecfdgii_api --parallel \
 && strip build/ecfdgii_api || true

# ---------------------------------------------------------------------------
# Runtime image
# ---------------------------------------------------------------------------

FROM ubuntu:24.04 AS final

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=America/Santo_Domingo

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates libpq5 curl tzdata \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /src/build/ecfdgii_api /app/ecfdgii_api
COPY --from=build /src/config/appsettings.json /app/appsettings.json
COPY --from=build /src/db/schema.sql /app/db/schema.sql
COPY --from=build ["/src/Documentación Técnica (XSD)", "/app/Documentación Técnica (XSD)"]

EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=5s --start-period=15s --retries=3 \
  CMD curl -f http://127.0.0.1:8080/health || exit 1

ENTRYPOINT ["/app/ecfdgii_api"]