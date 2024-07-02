#!/bin/sh
set -ev
cmake --version

# Enable or disable optional features?
if [ "$1" = "OFF" ]; then
	export OPTEN=OFF
else
	export OPTEN=ON
fi

OPTFEAT="-DENABLE_EXTRA_SECURITY=${OPTEN} -DENABLE_JPEG=${OPTEN} -DENABLE_XML=${OPTEN} -DENABLE_DECRYPTION=${OPTEN} -DENABLE_UNICE68=${OPTEN} -DENABLE_LIBMSPACK=${OPTEN} -DENABLE_PVRTC=${OPTEN} -DENABLE_ZSTD=${OPTEN} -DENABLE_LZ4=${OPTEN} -DENABLE_LZO=${OPTEN} -DENABLE_NLS=${OPTEN} -DENABLE_OPENMP=${OPTEN} -DTRACKER_INSTALL_API_VERSION=3"

mkdir build || true
cd build
cmake .. -G Ninja -DBUILD_TESTING=ON -DENABLE_LTO=OFF -DENABLE_PCH=OFF ${OPTFEAT}
