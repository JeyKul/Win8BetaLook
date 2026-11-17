#!/bin/bash
# Run a PowerShell command in the test VM, or copy files: vm.sh '<command>' | vm.sh put <files...> <C:/dest/>
H=test@192.168.122.27; K=(-i "$HOME/.ssh/win8look" -o LogLevel=ERROR -o BatchMode=yes)
if [ "$1" = put ]; then shift; d=${@: -1}; set -- "${@:1:$#-1}"; exec scp -q "${K[@]}" "$@" "$H:$d"; fi
if [ "$1" = get ]; then exec scp -q "${K[@]}" "$H:$2" "$3"; fi
exec ssh "${K[@]}" "$H" "$@"
