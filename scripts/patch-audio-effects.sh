#!/bin/sh
#
# Registers the Dolby AIDL effects in an AIDL audio_effects_config.xml.
#
# Usage: patch-audio-effects.sh <audio_effects_config.xml> [output.xml]
#
# Edits the file in place, or writes the result to output.xml and leaves
# the input untouched. Safe to run more than once: effects that are
# already present are skipped.
#
# UUIDs come from the previous HIDL audio_effects.xml of this device and
# from the strings of the pinned AIDL blobs. The effect factory only
# indexes an effect by the type given in this file, so type= is needed for
# the framework to find it: the AIDL "Extension" type for DAP, volume and
# game DAP (without it the app fails with error -3), and the spatializer
# type for the spatializer, whose type and UUID are the same. Only the DAP
# type is confirmed on a device; the others are not verified.
#
# The volume listener is also applied to the music stream after the
# existing music effects, as the old HIDL config did. The file must
# already have a <stream type="music"> in its <postprocess>: a second
# one would be rejected as a duplicate stream type.
set -e

IN="$1"
OUT="${2:-$1}"

[ -f "$IN" ] || { echo "usage: $0 <audio_effects_config.xml> [output.xml]" >&2; exit 1; }

TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT
cp "$IN" "$TMP"

grep -q '</libraries>' "$TMP" && grep -q '</effects>' "$TMP" || {
    echo "unexpected format: no </libraries> or </effects> in $IN" >&2
    exit 1
}

EXT_TYPE="fa81dbde-588b-11ed-9b6a-0242ac120002"

# add_effect <library name> <library file> <effect name> <uuid> [type]
add_effect() {
    lib_name="$1"; lib_file="$2"; eff_name="$3"; uuid="$4"; type="$5"

    grep -q "uuid=\"$uuid\"" "$TMP" && { echo "skip $eff_name (already registered)"; return; }

    if ! grep -q "path=\"$lib_file\"" "$TMP"; then
        sed -i "s#</libraries>#    <library name=\"$lib_name\" path=\"$lib_file\"/>\n    </libraries>#" "$TMP"
    fi

    # Use the library name registered in the file if it already has one
    lib_name="$(sed -n "s#.*<library name=\"\([^\"]*\)\" path=\"$lib_file\".*#\1#p" "$TMP" | head -n1)"

    attrs="name=\"$eff_name\" library=\"$lib_name\" uuid=\"$uuid\""
    [ -n "$type" ] && attrs="$attrs type=\"$type\""
    sed -i "s#</effects>#    <effect $attrs/>\n    </effects>#" "$TMP"
    echo "added $eff_name"
}

add_effect dolby_dap          libswdapaidl.so          dap          9d4921da-8225-4f29-aefa-39537a04bcaa "$EXT_TYPE"
add_effect dolby_vol          libdlbvolaidl.so         dlb_music_listener 40f66c8b-5aa5-4345-8919-53ec431aaa98 "$EXT_TYPE"
add_effect dolby_gamedap      libswgamedapaidl.so      gamedap      3783c334-d3a0-4d13-874f-0032e5fb80e2 "$EXT_TYPE"
add_effect dolby_spatializer  libswspatializeraidl.so  spatializer  ccd4cf09-a79d-46c2-9aae-06a1698d6c8f ccd4cf09-a79d-46c2-9aae-06a1698d6c8f

# Apply the volume listener on the music stream
if grep -q 'apply effect="dlb_music_listener"' "$TMP"; then
    echo "skip dlb_music_listener on music (already applied)"
else
    grep -q '<stream type="music">' "$TMP" || {
        echo "unexpected format: no music stream in $IN" >&2
        exit 1
    }
    sed -i '/<stream type="music">/,/<\/stream>/ s#^\( *\)</stream>#\1    <apply effect="dlb_music_listener"/>\n\1</stream>#' "$TMP"
    echo "applied dlb_music_listener on music"
fi

cp "$TMP" "$OUT"
echo "wrote $OUT"
