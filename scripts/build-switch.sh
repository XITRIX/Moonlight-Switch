#!/bin/env sh

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

docker build -t moonlight-switch -f ${SCRIPT_DIR}/switch.dockerfile ${SCRIPT_DIR} || exit 1

docker run -it --rm -v ${SCRIPT_DIR}/../:/code -e DEVKITPRO=/opt/devkitpro moonlight-switch 
