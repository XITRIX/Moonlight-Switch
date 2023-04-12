#!/bin/env sh

ROOT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

docker build -t moonlight-switch -f ${ROOT_DIR}/scripts/switch.dockerfile ${ROOT_DIR}/scripts || exit 1

docker run -it --rm -v ${ROOT_DIR}/:/code moonlight-switch
