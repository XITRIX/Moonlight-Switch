#!/bin/sh
set -eu

show_error() {
    if command -v kdialog >/dev/null 2>&1; then
        kdialog --title "Moonlight-Switch" --error "$1"
    elif command -v zenity >/dev/null 2>&1; then
        zenity --error --title="Moonlight-Switch" --text="$1"
    else
        echo "Moonlight-Switch: $1" >&2
    fi
}

show_success() {
    message="Moonlight-Switch was sent to Steam. Return to Game Mode and look in Library > Non-Steam."
    if command -v kdialog >/dev/null 2>&1; then
        kdialog --title "Moonlight-Switch" --msgbox "$message"
    elif command -v zenity >/dev/null 2>&1; then
        zenity --info --title="Moonlight-Switch" --text="$message"
    else
        echo "$message"
    fi
}

encode_uri_component() {
    if command -v python3 >/dev/null 2>&1; then
        python3 -c \
            'import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1], safe=""))' \
            "$1"
    elif command -v od >/dev/null 2>&1 &&
            command -v awk >/dev/null 2>&1; then
        # Percent-encoding every byte is valid and avoids requiring Python on
        # minimal gaming-focused distributions.
        printf '%s' "$1" |
            LC_ALL=C od -An -v -tx1 |
            awk '{ for (i = 1; i <= NF; i++) printf "%%%s", $i } END { print "" }'
    else
        return 1
    fi
}

open_steam_uri() {
    steam_uri="$1"

    if command -v steam >/dev/null 2>&1; then
        if steam "$steam_uri"; then
            return 0
        fi
    fi

    # Handle native clients whose launcher is omitted from PATH.
    user_home=${HOME:-}
    for steam_launcher in \
        "${user_home}/.steam/root/ubuntu12_32/steam" \
        "${user_home}/.local/share/Steam/ubuntu12_32/steam" \
        "${user_home}/.steam/steam/steam.sh" \
        "${user_home}/.local/share/Steam/steam.sh" \
        "/usr/bin/steam" \
        "/usr/bin/steam-runtime" \
        "/usr/bin/steam-native" \
        "/usr/games/steam"; do
        if [ -x "$steam_launcher" ]; then
            "$steam_launcher" "$steam_uri" >/dev/null 2>&1 &
            return 0
        fi
    done

    # Some immutable Linux gaming systems install Steam as a Flatpak.
    if command -v flatpak >/dev/null 2>&1 &&
            flatpak info com.valvesoftware.Steam >/dev/null 2>&1; then
        flatpak run com.valvesoftware.Steam "$steam_uri" >/dev/null 2>&1 &
        return 0
    fi

    # GIO honors x-scheme-handler/steam. Do not use xdg-open or KIO here:
    # KDE may treat steam:// as a KIO protocol and report a false success
    # before displaying "Unknown protocol 'steam'".
    if command -v gio >/dev/null 2>&1; then
        if gio open "$steam_uri"; then
            return 0
        fi
    fi

    return 1
}

script_path=$(readlink -f -- "$0")
app_dir=$(CDPATH= cd -- "$(dirname -- "$script_path")" && pwd)
shortcut="${app_dir}/Moonlight-Switch"

if [ ! -x "${app_dir}/AppRun" ]; then
    show_error "AppRun is missing or is not executable."
    exit 1
fi

# Use a descriptively named copy of AppRun so Steam creates a shortcut named
# Moonlight-Switch instead of the generic name AppRun.
if [ ! -x "$shortcut" ]; then
    show_error "The Steam shortcut launcher is missing or is not executable."
    exit 1
fi

if command -v steamos-add-to-steam >/dev/null 2>&1; then
    if ! steamos-add-to-steam -ui "$shortcut"; then
        show_error "SteamOS could not add the shortcut. Make sure Steam is running in Desktop Mode."
        exit 1
    fi
else
    if ! encoded_path=$(encode_uri_component "$shortcut"); then
        show_error "This system cannot encode the Steam shortcut path."
        exit 1
    fi
    touch /tmp/addnonsteamgamefile
    if ! open_steam_uri "steam://addnonsteamgame/${encoded_path}"; then
        show_error "Steam could not be reached. Make sure Steam is running in Desktop Mode."
        exit 1
    fi
fi

show_success
