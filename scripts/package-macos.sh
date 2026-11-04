#!/usr/bin/env bash
# macOS package of a built LYNX.app (shown as Buzz Beat Studio: CFBundleName): ad-hoc signature (LYNX has no Apple Developer ID, so it is NOT notarized) and a
# compressed .dmg with the app, a link to /Applications and a "read me first" note on how to open it the first time.
#
# usage: scripts/package-macos.sh <LYNX.app> <label> <output folder> [Demo.lynx]
set -euo pipefail
cd "$(dirname "$0")/.."

APP="$1"; LABEL="$2"; OUTDIR="$3"; DEMO="${4:-}"
NAME="Buzz-Beat-Studio-$LABEL-macos-universal"
STAGE="$(mktemp -d)/LYNX"
mkdir -p "$STAGE" "$OUTDIR"

ditto "$APP" "$STAGE/Buzz Beat Studio.app"
./scripts/fetch-onnxruntime.sh macos "$STAGE/Buzz Beat Studio.app/Contents/Frameworks"
# ad-hoc signature over the whole bundle: Apple Silicon refuses to run unsigned code, and a consistent bundle
# signature turns "LYNX is damaged" into the normal "unidentified developer" prompt
codesign --force --deep --sign - "$STAGE/Buzz Beat Studio.app"
codesign --verify --deep --strict "$STAGE/Buzz Beat Studio.app"
ln -s /Applications "$STAGE/Applications"
[[ -n "$DEMO" && -f "$DEMO" ]] && cp "$DEMO" "$STAGE/Demo.lynx"
cp LICENSE "$STAGE/LICENSE.txt"
cat > "$STAGE/Read me first.txt" <<'EOF'
Buzz Beat Studio for macOS (Apple Silicon and Intel, macOS 11 or newer)

1. Drag Buzz Beat Studio into the Applications folder.
2. Buzz Beat Studio is free software from an independent developer without an Apple Developer ID, so it is not notarized by
   Apple. The first time, open it like this:
     - Control-click (or right-click) Buzz Beat Studio in Applications, choose Open, then Open again.
     - On macOS 15 Sequoia or newer: try to open Buzz Beat Studio once, then go to System Settings > Privacy & Security and
       click "Open Anyway" next to the message about LYNX.
   Or, in Terminal:   xattr -dr com.apple.quarantine /Applications/Buzz\ Beat\ Studio.app
   After that it opens normally.
3. macOS asks for microphone access the first time you record.

Source code, checksums and help: https://github.com/musicnowdigital-code/LYNX-SHARE

---

Buzz Beat Studio para macOS (Apple Silicon e Intel, macOS 11 ou mais novo)

1. Arraste o Buzz Beat Studio para a pasta Aplicativos.
2. O Buzz Beat Studio é software livre de um desenvolvedor independente sem Apple Developer ID, por isso não é notarizado pela
   Apple. Na primeira vez, abra assim:
     - Clique com o botão direito (ou Control-clique) no Buzz Beat Studio em Aplicativos, escolha Abrir e depois Abrir de novo.
     - No macOS 15 Sequoia ou mais novo: tente abrir o Buzz Beat Studio uma vez, depois vá em Ajustes do Sistema > Privacidade e
       Segurança e clique em "Abrir Mesmo Assim" ao lado da mensagem sobre o LYNX.
   Ou, no Terminal:   xattr -dr com.apple.quarantine /Applications/Buzz\ Beat\ Studio.app
   Depois disso ele abre normalmente.
3. O macOS pede acesso ao microfone na primeira vez que você gravar.
EOF

rm -f "$OUTDIR/$NAME.dmg"
# hdiutil sometimes fails with "Resource busy" on CI runners (a transient lock on the image); retry a few times
for attempt in 1 2 3 4 5; do
    hdiutil create -volname "Buzz Beat Studio $LABEL" -srcfolder "$STAGE" -fs HFS+ -format UDZO -imagekey zlib-level=9 -ov \
        "$OUTDIR/$NAME.dmg" && break
    [[ $attempt -lt 5 ]] || exit 1
    echo "hdiutil create failed (attempt $attempt), retrying"; sleep $((attempt * 10))
done
hdiutil verify "$OUTDIR/$NAME.dmg"
ls -la "$OUTDIR/$NAME.dmg"
