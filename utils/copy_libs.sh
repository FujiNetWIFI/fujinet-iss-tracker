#!/bin/bash
#
# copies local dev version of fujinet-lib into _libs
#
# FUJINET_LIB_VERSION=4.2.1 TARGETS=c64 ./copy_libs.sh
#
# Requires FUJI_LIB_DIR to be the path to your fujinet-lib src directory

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR="${SCRIPT_DIR}/.."

if [ ! -d "${ROOT_DIR}/atari" ]; then
  echo "Could not determine ROOT_DIR, thought it was $ROOT_DIR, but was missing the atari subdir."
  exit 1
fi

if [ -z "${FUJI_LIB_DIR}" ]; then
  echo "You must have FUJI_LIB_DIR set to the location of fujinet-libs repo"
  exit 1
fi

if [ -z "${FUJINET_LIB_VERSION}" ]; then
  echo "You must set FUJINET_LIB_VERSION you wish to copy"
  exit 1
fi

TARGETS=${TARGETS:-"c64 atari apple2 apple2enh"}

LIBS_DIR=_libs

mkdir ${ROOT_DIR}/${LIBS_DIR} > /dev/null 2>&1

for target in ${TARGETS}; do
  ZIP_FILE=${FUJI_LIB_DIR}/dist/fujinet-lib-${target}-${FUJINET_LIB_VERSION}.zip

  if [ ! -f ${ZIP_FILE} ]; then
    echo "Couldn't find $ZIP_FILE"
    exit 1
  fi

  cp ${ZIP_FILE} ${ROOT_DIR}/${LIBS_DIR}
  rm -rf ${ROOT_DIR}/${LIBS_DIR}/${FUJINET_LIB_VERSION}-${target}
  unzip ${ZIP_FILE} -d ${ROOT_DIR}/${LIBS_DIR}/${FUJINET_LIB_VERSION}-${target}
done
