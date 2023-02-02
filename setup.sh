#!/bin/sh

# Usage: ./qemu_setup.sh [build_dir] [target-list] [extra_config_args...]

set -eux
BUILD_DIR="$1"
shift
mkdir -p "/root/libafl_qemu/${BUILD_DIR}";
cd "/root/libafl_qemu/${BUILD_DIR}";

TARGETS="$1"
shift

# only rebuild if config is missing
#if [ ! -e config-host.mak ]; then
  /root/libafl_qemu/configure \
                --target-list=${TARGETS} \
                --enable-system \
                --disable-slirp \
                --enable-plugins \
                --enable-fdt=internal \
                --audio-drv-list= \
                --disable-alsa \
                --disable-attr \
                --disable-auth-pam \
                --disable-dbus-display \
                --disable-bochs \
                --disable-bpf \
                --disable-brlapi \
                --disable-bsd-user \
                --disable-bzip2 \
                --disable-cap-ng \
                --disable-canokey \
                --disable-cloop \
                --disable-cocoa \
                --disable-coreaudio \
                --disable-curl \
                --disable-curses \
                --disable-dmg \
                --disable-docs \
                --disable-dsound \
                --disable-fuse \
                --disable-fuse-lseek \
                --disable-gcrypt \
                --disable-gettext \
                --disable-gio \
                --disable-glusterfs \
                --disable-gnutls \
                --disable-gtk \
                --disable-guest-agent \
                --disable-guest-agent-msi \
                --disable-hax \
                --disable-hvf \
                --disable-iconv \
                --disable-jack \
                --disable-keyring \
                --disable-kvm \
                --disable-libdaxctl \
                --disable-libiscsi \
                --disable-libnfs \
                --disable-libpmem \
                --disable-libssh \
                --disable-libudev \
                --disable-libusb \
                --disable-linux-aio \
                --disable-linux-io-uring \
                --disable-linux-user \
                --disable-live-block-migration \
                --disable-lzfse \
                --disable-lzo \
                --disable-l2tpv3 \
                --disable-malloc-trim \
                --disable-mpath \
                --disable-multiprocess \
                --disable-netmap \
                --disable-nettle \
                --disable-numa \
                --disable-nvmm \
                --disable-opengl \
                --disable-oss \
                --disable-pa \
                --disable-parallels \
                --disable-png \
                --disable-pvrdma \
                --disable-qcow1 \
                --disable-qed \
                --disable-qga-vss \
                --disable-rbd \
                --disable-rdma \
                --disable-replication \
                --disable-sdl \
                --disable-sdl-image \
                --disable-seccomp \
                --disable-selinux \
                --disable-slirp-smbd \
                --disable-smartcard \
                --disable-snappy \
                --disable-sparse \
                --disable-spice \
                --disable-spice-protocol \
                --disable-tools \
                --disable-tpm \
                --disable-usb-redir \
                --disable-user \
                --disable-u2f \
                --disable-vde \
                --disable-vdi \
                --disable-vduse-blk-export \
                --disable-vhost-crypto \
                --disable-vhost-kernel \
                --disable-vhost-net \
                --disable-vhost-user-blk-server \
                --disable-vhost-vdpa \
                --disable-virglrenderer \
                --disable-virtfs \
                --disable-virtiofsd \
                --disable-vmnet \
                --disable-vnc \
                --disable-vnc-jpeg \
                --disable-vnc-sasl \
                --disable-vte \
                --disable-vvfat \
                --disable-whpx \
                --disable-xen \
                --disable-xen-pci-passthrough \
                --disable-xkbcommon \
                --disable-zstd \
                --disable-capstone \
                --disable-sndio \
                $@
#fi

# Always make
make -j
