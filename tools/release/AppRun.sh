#!/bin/sh
# What the AppImage runs (packed as AppRun, at the top of its files): the editor, with the
# arguments it was given. Named first, one of the other programs runs instead:
#   redgem-linux-x86_64.AppImage                          the project hub
#   redgem-linux-x86_64.AppImage check my_game            any of the editor's commands
#   redgem-linux-x86_64.AppImage redgem-server --help     the game server (or redgem-relay,
#                                                         redgem-player, redgem-test)
# The programs sit in bin/ as in the tarball, so a build finds the players beside the editor.
set -eu

here="${APPDIR:-$(dirname "$(readlink -f "$0")")}"

case "${1:-}" in
  redgem-player | redgem-relay | redgem-server | redgem-test)
    program="$1"
    shift
    exec "${here}/bin/${program}" "$@"
    ;;
esac
exec "${here}/bin/redgem" "$@"
