# ---------------------------------------------------------------------------
# Multi-stage build for the C++ EcfDgii.Client REST host.
# ---------------------------------------------------------------------------
FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    git curl zip unzip tar ca-certificates \
    build-essential cmake ninja-build pkg-config \
    autoconf automake libtool python3 \
 && rm -rf /var/lib/apt/lists/*

# vcpkg
ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone https://github.com/microsoft/vcpkg "$VCPKG_ROOT" \
 && "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics

WORKDIR /src
COPY . .

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
