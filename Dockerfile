FROM alpine:latest

RUN echo "http://dl-4.alpinelinux.org/alpine/edge/testing" >> /etc/apk/repositories \
    && apk add --no-cache bash curl wget git nodejs npm python3-dev \
        build-base cmake autoconf libtool linux-headers pcre-dev gdb \
        gmp-dev procps-dev boost-dev openssl-dev pkgconfig py-pip \
    && pip3 install coverage py_ecc bitstring pysha3
