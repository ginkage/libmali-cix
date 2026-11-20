#!/bin/sh
# SPDX-License-Identifier: MIT
# Install the CIX P1 vendor GPU stack on Armbian (Ubuntu 26.04 "resolute", edge kernel) from the latest GitHub
# releases: cix-gpu-dkms (Mali kbase), libmali-cix (libmali + glue), mesa-cix (zink for desktop GL, replacing the
# installed Mesa packages) and xwayland +cixmali. Then switch to the vendor stack; reboot to use it.
#   sh install-cix-gpu.sh [-n]      -n: dry run (shows what apt would do, changes nothing)
set -eu
dry=; [ "${1:-}" = -n ] && dry=1
[ -n "$dry" ] || [ "$(id -u)" = 0 ] || exec sudo sh "$0" "$@"
dir=$(mktemp -d); trap 'rm -rf "$dir"' EXIT
fetch() { # repo, asset regex: download the newest release asset matching it
	url=$(curl -fsSL "https://api.github.com/repos/ginkage/$1/releases?per_page=20" | grep -o 'https://[^"]*\.deb' | grep -E "/$2" | head -1)
	[ -n "$url" ] || { echo "no $2 in ginkage/$1 releases" >&2; exit 1; }
	curl -fsSL -o "$dir/$(echo "${url##*/}" | sed 's/%2B/+/g')" "$url"
}
fetch mali-dkms-cix 'cix-gpu-dkms_'; fetch libmali-cix 'libmali-cix_'; fetch xserver 'xwayland_24\.'
for p in $(dpkg-query -W -f '${db:Status-Abbrev} ${Package} ${source:Package}\n' | awk '$1 == "ii" && $3 == "mesa" {print $2}'); do
	fetch mesa-cix "${p%%:*}_"   # every installed binary of the Mesa source: they are version-locked together
done
# DKMS needs the running kernel's headers: Armbian's package, at the installed kernel's version.
. /etc/armbian-release
hdr=; [ -d "/lib/modules/$(uname -r)/build" ] || hdr="linux-headers-$BRANCH-$LINUXFAMILY=$(dpkg-query -W -f '${Version}' "linux-image-$BRANCH-$LINUXFAMILY")"
if [ -n "$dry" ]; then ls "$dir"; apt-get -s install $hdr "$dir"/*.deb | grep -E '^(Inst|Remv)|upgraded'; exit; fi
# Keep these builds: a newer distro Mesa or Xwayland would otherwise replace them (and drop the CIX changes).
printf 'Package: src:mesa\nPin: version /\\+cix/\nPin-Priority: 1001\n\nPackage: xwayland\nPin: version /\\+cixmali/\nPin-Priority: 1001\n' > /etc/apt/preferences.d/cix-gpu.pref
apt-get install $hdr "$dir"/*.deb
mali-switch mali && echo "Reboot to start the vendor stack (sudo mali-switch panthor + reboot switches back)."
