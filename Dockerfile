ARG ARCH=armv7hf
ARG VERSION=12.0.0
ARG UBUNTU_VERSION=24.04
ARG REPO=axisecp
ARG SDK=acap-native-sdk

FROM ${REPO}/${SDK}:${VERSION}-${ARCH}-ubuntu${UBUNTU_VERSION}

# Set general arguments
ARG ARCH
ARG SDK_LIB_PATH_BASE=/opt/axis/acapsdk/sysroots/${ARCH}/usr
ARG BUILD_DIR=/opt/build

#-------------------------------------------------------------------------------
# Prepare build environment
#-------------------------------------------------------------------------------

# Install build dependencies for library cross compilation
RUN DEBIAN_FRONTEND=noninteractive \
    apt-get update && apt-get install -y -f --no-install-recommends \
    cmake && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

#-------------------------------------------------------------------------------
# Build libjpeg-turbo
#-------------------------------------------------------------------------------

ARG JPEGTURBO_VERSION=3.0.3
ARG JPEGTURBO_GIT_REPO=https://github.com/libjpeg-turbo/libjpeg-turbo
ARG JPEGTURBO_VERSION_DIR=libjpeg-turbo-${JPEGTURBO_VERSION}
ARG JPEGTURBO_DIR=${BUILD_DIR}/libjpeg-turbo
ARG JPEGTURBO_SRC_DIR=${JPEGTURBO_DIR}/${JPEGTURBO_VERSION_DIR}
ARG JPEGTURBO_BUILD_DIR=${JPEGTURBO_DIR}/build

WORKDIR ${JPEGTURBO_DIR}
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
RUN curl -fsSL ${JPEGTURBO_GIT_REPO}/releases/download/${JPEGTURBO_VERSION}/${JPEGTURBO_VERSION_DIR}.tar.gz | tar -xz

WORKDIR ${JPEGTURBO_SRC_DIR}
RUN . /opt/axis/acapsdk/environment-setup* && \
    pwd && ls -la && \
    cmake -G"Unix Makefiles" \
      -DCMAKE_INSTALL_PREFIX:PATH=${JPEGTURBO_BUILD_DIR} \
      . && \
      make -j && \
      make install

#-------------------------------------------------------------------------------
# Build uriparser library
#-------------------------------------------------------------------------------

ARG URIPARSER_VERSION=0.9.8
ARG URIPARSER_DIR=${BUILD_DIR}/uriparser
ARG URIPARSER_SRC_DIR=${BUILD_DIR}/uriparser-${URIPARSER_VERSION}
ARG URIPARSER_BUILD_DIR=${URIPARSER_DIR}/build

WORKDIR ${BUILD_DIR}
RUN curl -fsSL https://github.com/uriparser/uriparser/releases/download/uriparser-${URIPARSER_VERSION}/uriparser-${URIPARSER_VERSION}.tar.gz | tar -xz

WORKDIR ${URIPARSER_BUILD_DIR}
ENV COMMON_CMAKE_FLAGS="-S $URIPARSER_SRC_DIR \
        -B $URIPARSER_BUILD_DIR \
        -D CMAKE_INSTALL_PREFIX=$URIPARSER_BUILD_DIR \
        -D CMAKE_BUILD_TYPE=Release .. \
        -D URIPARSER_BUILD_DOCS=OFF \
        -D URIPARSER_BUILD_TESTS=OFF "

RUN if [ "$ARCH" = armv7hf ]; then \
        . /opt/axis/acapsdk/environment-setup* && \
        cmake \
        -D CMAKE_CXX_COMPILER=${CXX%-g++*}-g++ \
        -D CMAKE_CXX_FLAGS="${CXX#*-g++}" \
        -D CMAKE_C_COMPILER=${CC%-gcc*}-gcc \
        -D CMAKE_C_FLAGS="${CC#*-gcc}" \
        -D CPU_BASELINE=NEON,VFPV3 \
        -D ENABLE_NEON=ON \
        -D ENABLE_VFPV3=ON \
        $COMMON_CMAKE_FLAGS && \
        make -j "$(nproc)" install ; \
    elif [ "$ARCH" = aarch64 ]; then \
        . /opt/axis/acapsdk/environment-setup* && \
        cmake \
        -D CMAKE_CXX_COMPILER=${CXX%-g++*}-g++ \
        -D CMAKE_CXX_FLAGS="${CXX#*-g++}" \
        -D CMAKE_C_COMPILER=${CC%-gcc*}-gcc \
        -D CMAKE_C_FLAGS="${CC#*-gcc}" \
        $COMMON_CMAKE_FLAGS && \
        make -j "$(nproc)" install ; \
    else \
        printf "Error: '%s' is not a valid value for the ARCH variable\n", "$ARCH"; \
        exit 1; \
    fi

#-------------------------------------------------------------------------------
# Get models and labels
#-------------------------------------------------------------------------------

# Download pretrained models
WORKDIR /opt/app/model
ARG CHIP=
RUN if [ "$CHIP" = cpu ] || [ "$CHIP" = artpec8 ]; then \
        curl -L -o day_model.tflite \
            https://github.com/xSupsatien/Test-Model-TFLite/raw/main/day_model_v2.tflite && \
        curl -L -o night_model.tflite \
            https://github.com/xSupsatien/Test-Model-TFLite/raw/main/night_model_v2.tflite ; \
    elif [ "$CHIP" = edgetpu ]; then \
        curl -L -o converted_model.tflite \
            https://github.com/google-coral/test_data/raw/master/ssd_mobilenet_v2_coco_quant_postprocess_edgetpu.tflite ; \
    else \
        printf "Error: '%s' is not a valid value for the CHIP variable\n", "$CHIP"; \
        exit 1; \
    fi

WORKDIR /opt/app/label
RUN curl -L -o labels.txt https://github.com/xSupsatien/Test-Model-TFLite/raw/main/labelmap.txt

#-------------------------------------------------------------------------------
# Build ACAP application
#-------------------------------------------------------------------------------

WORKDIR /opt/app
COPY ./app .

# Copy required libraries
RUN mkdir -p lib && \
    cp ${JPEGTURBO_BUILD_DIR}/lib/*.so* lib/ && \
    cp ${URIPARSER_BUILD_DIR}/lib/liburiparser.so* lib/

RUN cp /opt/app/manifest.json.${CHIP} /opt/app/manifest.json && \
    . /opt/axis/acapsdk/environment-setup* && acap-build . \
    -a 'label/labels.txt' \
    -a 'model/day_model.tflite' \
    -a 'model/night_model.tflite'    