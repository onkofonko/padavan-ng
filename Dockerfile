FROM ubuntu:22.04

ENV PROJECT="padavan-ng"
ENV PROJECT_REPO="https://gitlab.com/hadzhioglu/${PROJECT}.git" \
    BASE_DIR="/opt" \
    DEBIAN_FRONTEND="noninteractive"

RUN apt update && \
    apt upgrade -y && \
    apt install --no-install-recommends -y \
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
        dos2unix \
        doxygen \
        fakeroot \
        flex \
        gawk \
        gettext \
        git \
        gperf \
        help2man \
        kmod \
        libarchive-tools \
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
        libidn2-dev \
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
        libunbound-dev \
        libvorbis-dev \
        libxml2-dev \
        locales \
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
        zlib1g-dev \
        zstd && \
    locale-gen --no-purge en_US.UTF-8 ru_RU.UTF-8 && \
    apt clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

ENV LANG="en_US.UTF-8" \
    LC_ALL="en_US.UTF-8"

WORKDIR "$BASE_DIR"
