FROM ubuntu:24.04 AS builder

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /source
COPY CMakeLists.txt ./
COPY simulator ./simulator

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel

FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends libstdc++6 \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /opt/exercise-tool/resources /results

COPY --from=builder /source/build/monet-exercise /opt/exercise-tool/monet-exercise
COPY --from=builder /source/build/resources /opt/exercise-tool/resources

WORKDIR /opt/exercise-tool
VOLUME ["/results"]

ENTRYPOINT ["/opt/exercise-tool/monet-exercise"]
