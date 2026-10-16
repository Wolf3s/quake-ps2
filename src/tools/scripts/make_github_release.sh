#!/usr/bin/env bash
#
# Builds the GitHub release packages from a clean tree: `make clean`, then
# `make` and `make release`, then github_rel_<version>/ at the repo root with
# one folder and one zip per config:
#
#   q1ps2_<config>_<version>/
#     quake.elf
#     quake_unstripped.elf  only with -u
#     LICENSE               the GPL, which the binaries must ship with
#     RELEASE_NOTES.txt     misc/RELEASE_NOTES.txt, @VERSION@ and @COMMIT@ filled in
#     id1/
#       config.cfg      the committed default (HEAD), not the working copy
#       music/          empty, for the player's trackNN.adp files
#
# The player adds the paks (id1/pak0.pak, and pak1.pak for the registered
# game). The zips are what gets attached to the GitHub release.

set -euo pipefail

usage() {
    cat <<EOF
Usage: $(basename "$0") [-f] [-u] [version]

  version   Release version. Defaults to the one the game reports, QuakeSpasm's
            (QUAKESPASM_VERSION, _VER_PATCH and _VER_SUFFIX in src/quake/quakedef.h).
  -f        Replace an existing github_rel_<version>/ folder.
  -u        Also package quake_unstripped.elf (the symbols, for addr2line).
EOF
}

die() {
    echo "error: $*" >&2
    exit 1
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$repo_root"

force=0
unstripped=0
version=""
for arg in "$@"; do
    case "$arg" in
        -f|--force) force=1 ;;
        -u|--unstripped) unstripped=1 ;;
        -h|--help)  usage; exit 0 ;;
        -*)         usage >&2; die "unknown option '$arg'" ;;
        *)
            [ -z "$version" ] || die "more than one version given"
            version="$arg"
            ;;
    esac
done

# QUAKESPASM_VER_STRING, as the console prints it: "0.97.0", plus the suffix
# when it is defined as a string literal ("-beta1"), not left empty.
if [ -z "$version" ]; then
    version="$(awk '
        $1 == "#define" && $2 == "QUAKESPASM_VERSION"    { major = $3 }
        $1 == "#define" && $2 == "QUAKESPASM_VER_PATCH"  { patch = $3 }
        $1 == "#define" && $2 == "QUAKESPASM_VER_SUFFIX" && $3 ~ /^"/ { suffix = $3; gsub(/"/, "", suffix) }
        END { if (major != "" && patch != "") print major "." patch suffix }
    ' src/quake/quakedef.h)"
    [ -n "$version" ] || die "no QUAKESPASM_VERSION in src/quake/quakedef.h; pass the version explicitly"
fi
[[ "$version" =~ ^[A-Za-z0-9._-]+$ ]] || die "version '$version' has characters unfit for a file name"

out_dir="github_rel_$version"
if [ -e "$out_dir" ] && [ "$force" -eq 0 ]; then
    die "$out_dir already exists; remove it or pass -f to replace it"
fi

release_notes="misc/RELEASE_NOTES.txt"
for file in LICENSE "$release_notes"; do
    [ -f "$file" ] || die "$file is missing"
done

commit="$(git rev-parse HEAD)"

# Not fatal: a release cut from uncommitted changes is sometimes intended.
if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
    echo "warning: uncommitted changes in tracked files; the ELFs are built from the working tree," \
         "not from commit $commit that the release notes point to." >&2
fi
if ! git diff --quiet HEAD -- id1/config.cfg; then
    echo "warning: id1/config.cfg differs from HEAD; the packages ship the committed one." >&2
fi

echo "==> make clean"
make clean
echo "==> make (debug)"
make || die "debug build failed"
echo "==> make release"
make release || die "release build failed"

elfs=(quake.elf)
[ "$unstripped" -eq 0 ] || elfs+=(quake_unstripped.elf)

for config in debug release; do
    for elf in "${elfs[@]}"; do
        [ -f "build/$config/$elf" ] || die "build/$config/$elf is missing after the build"
    done
done

# Only now that both builds succeeded, so a failed run leaves the old packages alone.
rm -rf "$out_dir"
mkdir -p "$out_dir"

for config in debug release; do
    name="q1ps2_${config}_${version}"
    pkg="$out_dir/$name"

    echo "==> packaging $name"
    mkdir -p "$pkg/id1/music"
    for elf in "${elfs[@]}"; do
        cp "build/$config/$elf" "$pkg/"
    done
    cp LICENSE "$pkg/"
    sed -e "s/@VERSION@/$version/g" -e "s/@COMMIT@/$commit/g" "$release_notes" > "$pkg/RELEASE_NOTES.txt"
    git show HEAD:id1/config.cfg > "$pkg/id1/config.cfg"

    (cd "$out_dir" && zip -r -9 -q "$name.zip" "$name" -x "*.DS_Store" -x "__MACOSX/*")
done

echo
echo "Release $version packaged in $out_dir/:"
ls -l "$out_dir"/*.zip
