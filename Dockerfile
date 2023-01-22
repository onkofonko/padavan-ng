FROM ubuntu:jammy

ENV PROJECT="padavan-ng"

ENV PROJECT_REPO="https://gitlab.com/timofeev36/${PROJECT}.git" \
    BASE_DIR="/opt" \
    DEBIAN_FRONTEND="noninteractive"

RUN apt update && \
    apt upgrade -y && \
    apt install -y \
    autoconf \
    autoconf-archive \
    automake \
    autopoint \
    bison \
    build-essential \
    ca-certificates \
    cmake \
    cpio \
    curl \
    doxygen \
    fakeroot \
    flex \
    gawk \
    gettext \
    git \
    gperf \
    help2man \
    htop \
    kmod \
    libblkid-dev \
    libc-ares-dev \
    libcurl4-openssl-dev \
    libdevmapper-dev \
    libev-dev \
    libevent-dev \
    libexif-dev \
    libflac-dev \
    libgmp3-dev \
    libid3tag0-dev \
    libjpeg-dev \
    libkeyutils-dev \
    libltdl-dev \
    libmpc-dev \
    libmpfr-dev \
    libncurses5-dev \
    libogg-dev \
    libsqlite3-dev \
    libssl-dev \
    libtool \
    libtool-bin \
    libudev-dev \
    libvorbis-dev \
    libxml2-dev \
    locales \
    mc \
    nano \
    pkg-config \
    ppp-dev \
    python3 \
    python3-docutils \
    texinfo \
    unzip \
    uuid \
    uuid-dev \
    vim \
    wget \
    xxd \
    zlib1g-dev

RUN locale-gen --no-purge en_US.UTF-8 ru_RU.UTF-8

ENV LANG="en_US.UTF-8" \
    LC_ALL="en_US.UTF-8"

WORKDIR "$BASE_DIR"

RUN git clone "$PROJECT_REPO" --depth=1 && \
    cd "${PROJECT}/toolchain" && \
    ./clean_sources.sh && \
    ./build_toolchain.sh
