#!/usr/bin/env bash

set -e

CPU_COUNT=$(lscpu -p | sed '/^#/ d' | wc -l)
MAKE="make -j${CPU_COUNT}"

WORKDIR=${1-/home/vagrant/tmp/bin}
INSTALL_PREFIX=${2-/usr}
PATCHESDIR=${3-/vagrant}

BOOST_MAJOR="1"
BOOST_MINOR="61"
BOOST_PATCH="0"
BOOST_VERSION="${BOOST_MAJOR}_${BOOST_MINOR}_${BOOST_PATCH}"
BOOST_VERSION_DOTTED="${BOOST_MAJOR}.${BOOST_MINOR}.${BOOST_PATCH}"
BOOST_DIR_NAME="boost_${BOOST_VERSION}"
BOOST_ARCHIVE_NAME="boost_${BOOST_VERSION}.tar.gz"
BOOST_URL="http://sourceforge.net/projects/boost/files/boost/${BOOST_VERSION_DOTTED}/${BOOST_ARCHIVE_NAME}"
if [ ! -e "${INSTALL_PREFIX}/lib/libboost_system.so" ]; then
    if [ ! -e "${WORKDIR}/${BOOST_ARCHIVE_NAME}" ]; then
        mkdir -p "${WORKDIR}/download"
        wget -q -P "${WORKDIR}/download" "${BOOST_URL}"
        mv "${WORKDIR}/download/${BOOST_ARCHIVE_NAME}" "${WORKDIR}"
    fi
    # tar is also loud
    tar -C "${WORKDIR}" -zxf "${WORKDIR}/${BOOST_ARCHIVE_NAME}"
    pushd "${WORKDIR}/${BOOST_DIR_NAME}"
    # boost build scripts are really loud
    ./bootstrap.sh --prefix="${INSTALL_PREFIX}"
    ./b2 "-j${CPU_COUNT}" install
    popd
fi

echo "Finished boost"

# libuv
LIBUV_VERSION="1.6.1"
LIBUV_DIR_NAME="libuv-v${LIBUV_VERSION}"
LIBUV_ARCHIVE_NAME="libuv-v${LIBUV_VERSION}.tar.gz"
LIBUV_URL="http://dist.libuv.org/dist/v${LIBUV_VERSION}/${LIBUV_ARCHIVE_NAME}"
if [ ! -e "${INSTALL_PREFIX}/lib64/libuv.so" ] && [ ! -e "${INSTALL_PREFIX}/lib/libuv.so" ]; then
    if [ ! -e "${WORKDIR}/${LIBUV_ARCHIVE_NAME}" ]; then
        mkdir -p "${WORKDIR}/download"
        wget -q -P "${WORKDIR}/download" "${LIBUV_URL}"
        mv "${WORKDIR}/download/${LIBUV_ARCHIVE_NAME}" "${WORKDIR}"
    fi
    tar -C "${WORKDIR}" -zxf "${WORKDIR}/${LIBUV_ARCHIVE_NAME}"
    pushd "${WORKDIR}/${LIBUV_DIR_NAME}"
    sh autogen.sh
    ./configure --prefix="${INSTALL_PREFIX}"
    ${MAKE}
    ${MAKE} install
    popd
fi

echo "Finished libuv"

# AMQP-CPP
AMQP_CPP_VERSION="2.6.2"
AMQP_CPP_DIR_NAME="AMQP-CPP-${AMQP_CPP_VERSION}"
AMQP_CPP_ARCHIVE_NAME="v${AMQP_CPP_VERSION}.tar.gz"
AMQP_CPP_URL="https://github.com/CopernicaMarketingSoftware/AMQP-CPP/archive/v${AMQP_CPP_VERSION}.tar.gz"

if [ ! -e "${INSTALL_PREFIX}/lib/libamqpcpp.so" ]; then
    if [ ! -e "${WORKDIR}/${AMQP_CPP_ARCHIVE_NAME}" ]; then
        mkdir -p "${WORKDIR}/download"
        wget -q -P "${WORKDIR}/download" "${AMQP_CPP_URL}"
        mv "${WORKDIR}/download/${AMQP_CPP_ARCHIVE_NAME}" "${WORKDIR}"
    fi
    tar -C "${WORKDIR}" -zxf "${WORKDIR}/${AMQP_CPP_ARCHIVE_NAME}"
    pushd "${WORKDIR}/${AMQP_CPP_DIR_NAME}"
    ${MAKE}
    ${MAKE} install PREFIX="${INSTALL_PREFIX}"
    ldconfig
    popd
fi

echo "Finished amqp-cpp"

# folly
FOLLY_VERSION="e28213bf2c5aa1c400cfd2ab363abfeadee81fa2"
FOLLY_DIR_NAME="folly-${FOLLY_VERSION}"
FOLLY_ARCHIVE_NAME="v${FOLLY_VERSION}.tar.gz"
FOLLY_URL="https://github.com/facebook/folly/archive/v${FOLLY_VERSION}.tar.gz"

if [ ! -e "${INSTALL_PREFIX}/lib64/libfolly.so" ] && [ ! -e "${INSTALL_PREFIX}/lib/libfolly.so" ]; then
    if [ ! -e "${WORKDIR}/folly" ]; then
        mkdir -p "${WORKDIR}/folly"
        git clone https://github.com/facebook/folly.git "${WORKDIR}/folly"
    fi
    pushd "${WORKDIR}/folly"
    git checkout "${FOLLY_VERSION}"
    mkdir -p patches
    cp ${PATCHESDIR}/patches/folly/* patches/
    git apply patches/*
    pushd folly
    autoreconf -ivf
    ./configure --prefix="${INSTALL_PREFIX}"
    ${MAKE}
    ${MAKE} install
    popd
    popd
fi

echo "Finished Folly"

# protobuf

PROTOBUF_VERSION="2.6.1"
PROTOBUF_DIR_NAME="protobuf-${PROTOBUF_VERSION}"
PROTOBUF_ARCHIVE_NAME="protobuf-${PROTOBUF_VERSION}.tar.gz"
PROTOBUF_URL="https://github.com/google/protobuf/releases/download/v${PROTOBUF_VERSION}/${PROTOBUF_ARCHIVE_NAME}"
if [ ! -e "${INSTALL_PREFIX}/bin/protoc" ]; then
    if [ ! -e "${WORKDIR}/${PROTOBUF_ARCHIVE_NAME}" ]; then
        mkdir -p "${WORKDIR}/download"
        wget -q -P "${WORKDIR}/download" "${PROTOBUF_URL}"
        mv "${WORKDIR}/download/${PROTOBUF_ARCHIVE_NAME}" "${WORKDIR}"
    fi
    tar -C "${WORKDIR}" -zxf "${WORKDIR}/${PROTOBUF_ARCHIVE_NAME}"
    pushd "${WORKDIR}/${PROTOBUF_DIR_NAME}"
    ./configure --prefix="${INSTALL_PREFIX}"
    ${MAKE}
    ${MAKE} install
    popd
fi

echo "Finished yaml-cpp"

# yaml-cpp
YAML_CPP_VERSION="0.5.2"
YAML_CPP_DIR_NAME="yaml-cpp-release-${YAML_CPP_VERSION}"
YAML_CPP_ARCHIVE_NAME="release-${YAML_CPP_VERSION}.tar.gz"
YAML_CPP_URL="https://github.com/jbeder/yaml-cpp/archive/${YAML_CPP_ARCHIVE_NAME}"

if [ ! -e "${INSTALL_PREFIX}/lib/libyaml-cpp.a" ]; then
    if [ ! -e "${WORKDIR}/${YAML_CPP_ARCHIVE_NAME}" ]; then
        mkdir -p "${WORKDIR}/download"
        wget -q -P "${WORKDIR}/download" "${YAML_CPP_URL}"
        mv "${WORKDIR}/download/${YAML_CPP_ARCHIVE_NAME}" "${WORKDIR}"
    fi
    tar -C "${WORKDIR}" -zxf "${WORKDIR}/${YAML_CPP_ARCHIVE_NAME}"
    pushd "${WORKDIR}/${YAML_CPP_DIR_NAME}"
    mkdir -p patches
    cp ${PATCHESDIR}/patches/yaml-cpp/* patches/
    git apply patches/*
    cmake -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" .
    ${MAKE}
    ${MAKE} install
    popd
fi

echo "Finished dependencies"
