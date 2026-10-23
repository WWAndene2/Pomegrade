#!/bin/bash
# A diagnostic copy of a built mod with some files or archive members swapped (23 October: a day of tests lost to
# hand-edited copies, a GARC rewritten by hand with a stale data size, and copies that filled the disk). The copy replaces
# <variant dir> (one per test series, so they do not pile up); each change is checked by reading it back.
#
#   tools/remake/headless/mod_variant.sh <work dir> <base mod> <variant dir> CHANGE...
#
# CHANGE:
#   PATH=FILE          the romfs file PATH (a/0/3/4, DllBattle.cro...) made FILE, whole
#   PATH:N=FILE        member N of the archive PATH (as the variant holds it so far, from the base mod: remake_tool mod-file) made FILE; N one
#                      past the last appends it (remake_tool garc-set)
# A file the mod patched (romfs_ext/PATH.bps) is then given whole in romfs/ and its patch removed. Run oras-engine on the
# base mod before, never on the variant (it would write the engine's files back over the changes). The variant's title
# state is a new one (fast_run.sh keys it by the mod's content): its first run boots once more, and random rolls differ
# from the base mod's runs: for a wild battle use retro_host's "until log:DllBattle MAX left:16,right:16" (walks until it).
set -euo pipefail
work=$(cd "$1" && pwd); base=$2; variant=$3; shift 3
tool=$work/build-remake/remake_tool; game=$work/dumps/oras.3ds
[ -d "$base/load" ] || { echo "no mod at $base"; exit 1; }
case "$(cd "$(dirname "$variant")" && pwd)/$(basename "$variant")" in
    "$work"/*) ;; *) echo "the variant must be inside the work dir"; exit 1 ;;
esac
rm -rf "$variant"
cp -a "$base" "$variant"
mods=$(ls -d "$variant"/load/mods/*/ | head -1)
tmp=$(mktemp -d "$work/variant.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
for change in "$@"; do
    target=${change%%=*}; file=${change#*=}
    [ -f "$file" ] || { echo "no file $file"; exit 1; }
    path=${target%%:*}
    if [ "$path" != "$target" ]; then
        member=${target#*:}
        "$tool" mod-file "$game" "$variant" "$path" "$tmp/archive" > /dev/null  # the variant as it stands: two changes to one archive both kept
        "$tool" garc-set "$tmp/archive" "$tmp/new" "$member" "$file" | tail -1
        "$tool" garc "$tmp/new" "$tmp/check" > /dev/null
        cmp -s "$(ls "$tmp/check/$member".* | head -1)" "$file" || { echo "member $member of $path does not read back"; exit 1; }
        rm -rf "$tmp/check"
        file=$tmp/new
    fi
    mkdir -p "$mods/romfs/$(dirname "$path")"
    cp "$file" "$mods/romfs/$path"
    rm -f "$mods/romfs_ext/$path.bps"
    echo "$path${member:+ member $member}: $change"
    unset member
done
