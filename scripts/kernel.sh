#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
checkout=$1
make_command=$2
pin=7d5d574e8643c3645fa3ce2a126dcbd4ae43662f
if [ ! -e "$checkout" ]; then
    git clone --depth 1 --branch v1.0.0 https://github.com/Evan-Windproof/Liberums.git "$checkout"
fi
if [ "$(git -C "$checkout" rev-parse 'HEAD^{commit}')" != "$pin" ] ||
   [ -n "$(git -C "$checkout" status --porcelain --untracked-files=no)" ]; then
    printf '%s\n' "Expected clean Liberums $pin in $checkout" >&2
    exit 1
fi
# Keep the pinned source intact and build only its kernel and kernel fixtures.
exec "$make_command" -C "$checkout" all
