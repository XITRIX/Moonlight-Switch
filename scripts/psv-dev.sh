#!/usr/bin/env bash

set -Eeuo pipefail

PSV_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PSV_PROJECT_ROOT="$(cd "${PSV_SCRIPT_DIR}/.." && pwd)"
PSV_BUILD_DIR="${PSV_BUILD_DIR:-${PSV_PROJECT_ROOT}/build-psv}"
PSV_BUILD_TYPE="${PSV_BUILD_TYPE:-Release}"
PSV_IP="${PSV_IP:-192.168.1.209}"
PSV_TITLE_ID="${PSV_TITLE_ID:-MNTL00000}"
PSV_FTP_PORT="${PSV_FTP_PORT:-1337}"
PSV_COMMAND_PORT="${PSV_COMMAND_PORT:-1338}"
PSV_LOG_PORT="${PSV_LOG_PORT:-9999}"
PSV_HEALTH_TIMEOUT="${PSV_HEALTH_TIMEOUT:-45}"
PSV_STABILITY_SECONDS="${PSV_STABILITY_SECONDS:-8}"
PSV_CONNECT_TIMEOUT="${PSV_CONNECT_TIMEOUT:-3}"
PSV_TRANSFER_TIMEOUT="${PSV_TRANSFER_TIMEOUT:-180}"
PSV_DEPLOY_RESOURCES="${PSV_DEPLOY_RESOURCES:-0}"
PSV_LOG_DIR="${PSV_LOG_DIR:-${PSV_PROJECT_ROOT}/.psv-logs}"
PSV_HEALTH_MARKER="VITA_HEALTH: READY"
PSV_FTP_BASE="ftp://${PSV_IP}:${PSV_FTP_PORT}"

PSV_LOGGER_PID=""
PSV_LOG_FILE=""
PSV_CRASH_SNAPSHOT=""

psv_log() {
    printf '[psv-dev] %s\n' "$*"
}

psv_die() {
    printf '[psv-dev] ERROR: %s\n' "$*" >&2
    exit 1
}

psv_usage() {
    cat <<'EOF'
Usage: scripts/psv-dev.sh [doctor|build|install|cycle|logs|parse-crash]

  doctor       Check the local toolchain and Vita development services.
  build        Configure and build build-psv/Moonlight.vpk.
  install      Build and upload the VPK to ux0:/data/Moonlight.vpk.
  cycle        Build, deploy eboot.bin, launch, and verify healthy logs.
  logs         Listen for PrincessLog output on the configured log port.
  parse-crash  Download and symbolize the newest Vita crash with Docker.

The default action is cycle. Set PSV_DEPLOY_RESOURCES=1 when resources or
Vita runtime modules changed. See docs/psv-development.md for setup details.
EOF
}

psv_cleanup() {
    if [[ -n "${PSV_LOGGER_PID}" ]] && kill -0 "${PSV_LOGGER_PID}" 2>/dev/null; then
        kill "${PSV_LOGGER_PID}" 2>/dev/null || true
        wait "${PSV_LOGGER_PID}" 2>/dev/null || true
    fi
    if [[ -n "${PSV_CRASH_SNAPSHOT}" ]]; then
        rm -f "${PSV_CRASH_SNAPSHOT}"
    fi
}
trap psv_cleanup EXIT INT TERM

psv_need_command() {
    command -v "$1" >/dev/null 2>&1 || psv_die "Missing required command: $1"
}

psv_detect_jobs() {
    local psv_jobs
    psv_jobs="$(sysctl -n hw.ncpu 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || printf '4')"
    printf '%s' "${PSV_BUILD_JOBS:-${psv_jobs}}"
}

psv_local_ip() {
    local psv_interface
    psv_interface="$(route -n get "${PSV_IP}" 2>/dev/null | awk '/interface:/{print $2; exit}')"
    if [[ -n "${psv_interface}" ]] && command -v ipconfig >/dev/null 2>&1; then
        ipconfig getifaddr "${psv_interface}" 2>/dev/null || true
    fi
}

psv_port_open() {
    if [[ "$(uname -s)" == Darwin ]]; then
        nc -4 -z -G "${PSV_CONNECT_TIMEOUT}" "${PSV_IP}" "$1" >/dev/null 2>&1
    else
        nc -4 -z -w "${PSV_CONNECT_TIMEOUT}" "${PSV_IP}" "$1" >/dev/null 2>&1
    fi
}

psv_ftp_list() {
    local psv_directory="$1"
    curl --fail --silent --show-error --disable-epsv \
        --quote "CWD ${psv_directory}" \
        --connect-timeout "${PSV_CONNECT_TIMEOUT}" \
        --max-time "${PSV_TRANSFER_TIMEOUT}" \
        "${PSV_FTP_BASE}/"
}

psv_ftp_upload() {
    local psv_source="$1"
    local psv_destination="$2"
    local psv_directory="${psv_destination%/*}"
    local psv_filename="${psv_destination##*/}"

    [[ -n "${psv_directory}" && -n "${psv_filename}" ]] || \
        psv_die "Invalid Vita upload destination: ${psv_destination}"

    curl --fail --silent --show-error --disable-epsv \
        --quote "CWD ${psv_directory}" \
        --connect-timeout "${PSV_CONNECT_TIMEOUT}" \
        --max-time "${PSV_TRANSFER_TIMEOUT}" \
        --upload-file "${psv_source}" \
        "${PSV_FTP_BASE}/${psv_filename}"
}

psv_send_command() {
    local psv_command="$1"
    if [[ "$(uname -s)" == Darwin ]]; then
        printf '%s\n' "${psv_command}" | nc -4 -G "${PSV_CONNECT_TIMEOUT}" \
            -w "${PSV_CONNECT_TIMEOUT}" "${PSV_IP}" "${PSV_COMMAND_PORT}"
    else
        printf '%s\n' "${psv_command}" | nc -4 -w "${PSV_CONNECT_TIMEOUT}" \
            "${PSV_IP}" "${PSV_COMMAND_PORT}"
    fi
}

psv_check_local() {
    psv_need_command cmake
    psv_need_command ninja
    psv_need_command curl
    psv_need_command nc

    if [[ -z "${VITASDK:-}" ]] && [[ -d /opt/vitasdk ]]; then
        export VITASDK=/opt/vitasdk
    fi
    [[ -n "${VITASDK:-}" ]] || psv_die 'VITASDK is not set'
    [[ -f "${VITASDK}/share/vita.toolchain.cmake" ]] || \
        psv_die "Invalid VITASDK path: ${VITASDK}"
    [[ -f "${VITASDK}/arm-vita-eabi/lib/libSDL2.a" ]] || \
        psv_die 'VitaSDK SDL2 dependency is missing'
    grep -Eq '^#define SDL_VIDEO_VITA_PVR 1$' \
        "${VITASDK}/arm-vita-eabi/include/SDL2/SDL_config.h" || \
        psv_die 'VitaSDK SDL2 was built without VIDEO_VITA_PVR support'
    [[ -f "${VITASDK}/arm-vita-eabi/lib/libavcodec.a" ]] || \
        psv_die 'VitaSDK FFmpeg dependency is missing'
    [[ -f "${PSV_PROJECT_ROOT}/extern/borealis/psv/module/libpvrPSP2_WSEGL.suprx" ]] || \
        psv_die 'Borealis Vita window-system module is missing'
}

psv_check_device() {
    local psv_host_ip
    psv_host_ip="$(psv_local_ip)"
    [[ -z "${psv_host_ip}" ]] || psv_log "PrincessLog host should be ${psv_host_ip}:${PSV_LOG_PORT}"

    psv_port_open "${PSV_FTP_PORT}" || \
        psv_die "Vita FTP port ${PSV_FTP_PORT} is unavailable; enable vitacompanion and wake the Vita"
    psv_port_open "${PSV_COMMAND_PORT}" || \
        psv_die "Vita command port ${PSV_COMMAND_PORT} is unavailable; enable vitacompanion and wake the Vita"
}

psv_build() {
    psv_check_local
    psv_log "Configuring Vita build in ${PSV_BUILD_DIR}"
    cmake -S "${PSV_PROJECT_ROOT}" -B "${PSV_BUILD_DIR}" -G Ninja \
        -DPLATFORM_PSV=ON \
        -DCMAKE_BUILD_TYPE="${PSV_BUILD_TYPE}"

    psv_log 'Building Moonlight.vpk'
    cmake --build "${PSV_BUILD_DIR}" --target Moonlight.vpk \
        --parallel "$(psv_detect_jobs)"

    [[ -f "${PSV_BUILD_DIR}/Moonlight.vpk" ]] || psv_die 'VPK build did not produce Moonlight.vpk'
    [[ -f "${PSV_BUILD_DIR}/Moonlight.self" ]] || psv_die 'VPK build did not produce Moonlight.self'
}

psv_app_is_installed() {
    psv_ftp_list "ux0:/app/${PSV_TITLE_ID}/" >/dev/null 2>&1
}

psv_upload_install_vpk() {
    psv_log 'Uploading VPK to ux0:/data/Moonlight.vpk'
    psv_ftp_upload "${PSV_BUILD_DIR}/Moonlight.vpk" 'ux0:/data/Moonlight.vpk'
    psv_log 'Upload complete. Install ux0:/data/Moonlight.vpk once in VitaShell, then run cycle.'
}

psv_deploy_resources() {
    local psv_source
    local psv_relative
    psv_log 'Deploying resources and Vita runtime modules'
    while IFS= read -r -d '' psv_source; do
        if [[ "${psv_source}" == "${PSV_PROJECT_ROOT}/resources/"* ]]; then
            psv_relative="resources/${psv_source#${PSV_PROJECT_ROOT}/resources/}"
        elif [[ "${psv_source}" == "${PSV_PROJECT_ROOT}/app/platforms/psv/module/"* ]]; then
            psv_relative="module/${psv_source#${PSV_PROJECT_ROOT}/app/platforms/psv/module/}"
        else
            psv_relative="module/${psv_source#${PSV_PROJECT_ROOT}/extern/borealis/psv/module/}"
        fi
        psv_ftp_upload "${psv_source}" "ux0:/app/${PSV_TITLE_ID}/${psv_relative}"
    done < <(find "${PSV_PROJECT_ROOT}/resources" \
                   "${PSV_PROJECT_ROOT}/app/platforms/psv/module" \
                   "${PSV_PROJECT_ROOT}/extern/borealis/psv/module/libpvrPSP2_WSEGL.suprx" \
                   -type f ! -name '.DS_Store' -print0)
}

psv_snapshot_crashes() {
    psv_ftp_list 'ux0:/data/' 2>/dev/null | \
        grep -E 'psp2core-.*\.(psp2dmp|psp2dump)(\.tmp)?$' | sort || true
}

psv_start_logger() {
    mkdir -p "${PSV_LOG_DIR}"
    PSV_LOG_FILE="${PSV_LOG_DIR}/$(date '+%Y%m%d-%H%M%S').log"
    : > "${PSV_LOG_FILE}"
    nc -4 -lk "${PSV_LOG_PORT}" > "${PSV_LOG_FILE}" 2>&1 &
    PSV_LOGGER_PID=$!
    sleep 1
    kill -0 "${PSV_LOGGER_PID}" 2>/dev/null || \
        psv_die "Could not listen on TCP ${PSV_LOG_PORT}; another logger may be running"
    psv_log "Capturing PrincessLog output in ${PSV_LOG_FILE}"
}

psv_new_crash_dump() {
    local psv_before="$1"
    local psv_after="$2"
    comm -13 "${psv_before}" "${psv_after}" | grep -q .
}

psv_check_health() {
    local psv_crashes_before="$1"
    local psv_crashes_after
    local psv_ready_at=0
    local psv_deadline=$((SECONDS + PSV_HEALTH_TIMEOUT))
    local psv_error_pattern='\[(ERROR|FATAL)\]|Segmentation fault|C2-12828-1|assert(ion)? failed|abort(ed)?|VITA_HEALTH: FAILED'

    psv_crashes_after="$(mktemp)"
    while (( SECONDS < psv_deadline )); do
        if grep -Eiq "${psv_error_pattern}" "${PSV_LOG_FILE}"; then
            psv_log 'Unhealthy error marker detected:'
            grep -Ein "${psv_error_pattern}" "${PSV_LOG_FILE}" | tail -n 20 >&2
            rm -f "${psv_crashes_after}"
            return 1
        fi

        if (( psv_ready_at == 0 )) && grep -Fq "${PSV_HEALTH_MARKER}" "${PSV_LOG_FILE}"; then
            psv_ready_at=${SECONDS}
            psv_log "Ready marker received; observing for ${PSV_STABILITY_SECONDS}s"
        fi

        psv_snapshot_crashes > "${psv_crashes_after}"
        if psv_new_crash_dump "${psv_crashes_before}" "${psv_crashes_after}"; then
            psv_log 'A new Vita crash dump appeared:'
            comm -13 "${psv_crashes_before}" "${psv_crashes_after}" >&2
            rm -f "${psv_crashes_after}"
            return 1
        fi

        if (( psv_ready_at > 0 && SECONDS - psv_ready_at >= PSV_STABILITY_SECONDS )); then
            rm -f "${psv_crashes_after}"
            psv_log "Healthy: ${PSV_HEALTH_MARKER} and no errors or new crash dumps"
            return 0
        fi
        sleep 1
    done

    rm -f "${psv_crashes_after}"
    psv_log "Timed out waiting for '${PSV_HEALTH_MARKER}'"
    psv_log "Check PrincessLog's host/port and inspect ${PSV_LOG_FILE}"
    return 1
}

psv_cycle() {
    PSV_CRASH_SNAPSHOT="$(mktemp)"

    psv_build
    psv_check_device

    if ! psv_app_is_installed; then
        psv_upload_install_vpk
        psv_die "${PSV_TITLE_ID} is not installed"
    fi

    psv_snapshot_crashes > "${PSV_CRASH_SNAPSHOT}"
    psv_log "Stopping ${PSV_TITLE_ID} before deployment"
    psv_send_command "kill ${PSV_TITLE_ID}" >/dev/null 2>&1 || true

    psv_log 'Deploying Moonlight.self as eboot.bin'
    psv_ftp_upload "${PSV_BUILD_DIR}/Moonlight.self" \
        "ux0:/app/${PSV_TITLE_ID}/eboot.bin"

    if [[ "${PSV_DEPLOY_RESOURCES}" == 1 ]]; then
        psv_deploy_resources
    fi

    psv_start_logger
    psv_log "Launching ${PSV_TITLE_ID}"
    psv_send_command "launch ${PSV_TITLE_ID}" >/dev/null

    if ! psv_check_health "${PSV_CRASH_SNAPSHOT}"; then
        psv_log "Latest log output:"
        tail -n 80 "${PSV_LOG_FILE}" >&2 || true
        psv_die 'Vita health check failed'
    fi
}

psv_logs() {
    mkdir -p "${PSV_LOG_DIR}"
    PSV_LOG_FILE="${PSV_LOG_DIR}/$(date '+%Y%m%d-%H%M%S').log"
    psv_log "Listening on TCP ${PSV_LOG_PORT}; writing ${PSV_LOG_FILE}"
    nc -4 -lk "${PSV_LOG_PORT}" | tee "${PSV_LOG_FILE}"
}

psv_parse_crash() {
    psv_need_command docker
    psv_check_device
    [[ -f "${PSV_BUILD_DIR}/Moonlight" ]] || psv_die 'Build Moonlight before parsing a crash'
    docker run --rm -v "${PSV_BUILD_DIR}:/src" xfangfang/vita_parse \
        "${PSV_FTP_BASE}" Moonlight
}

psv_doctor() {
    psv_check_local
    psv_log "VitaSDK: ${VITASDK}"
    psv_check_device
    psv_log 'Local toolchain and Vita development services are ready'
}

PSV_ACTION="${1:-cycle}"
case "${PSV_ACTION}" in
    doctor) psv_doctor ;;
    build) psv_build ;;
    install)
        psv_build
        psv_check_device
        psv_upload_install_vpk
        ;;
    cycle) psv_cycle ;;
    logs) psv_logs ;;
    parse-crash) psv_parse_crash ;;
    -h|--help|help) psv_usage ;;
    *)
        psv_usage >&2
        psv_die "Unknown action: ${PSV_ACTION}"
        ;;
esac
