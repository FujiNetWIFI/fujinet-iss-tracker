#!/bin/bash

function show_help {
  echo "Usage: $(basename "$0") DISK.po HGR_FILE"
  echo "  Adds HGR_FILE to po"
  exit 1
}

if [ -z "$(which java)" ]; then
  echo "Unable to find java on command line. You must have a working java at least version 11 to use this script."
  exit 1
fi

DISKFILE="$1"
if [ ! -f "$DISKFILE" ]; then
  echo "Unable to find target DISK."
  exit 1
fi

HGR_FILE="$2"
if [ ! -f "$HGR_FILE" ]; then
  echo "Unable to find HGR image file."
  exit 1
fi

SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
source "$SCRIPT_DIR/get-binaries.sh"

${AC} -p ${DISKFILE} $(basename $HGR_FILE) bin < ${HGR_FILE}
