#!/bin/sh
# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2
#
# Installe des paquets Ubuntu depuis un instantané daté de l'archive (snapshot.ubuntu.com),
# pour que deux exécutions installent les mêmes versions.
#
# Usage : APT_SNAPSHOT=AAAAMMJJTHHMMSSZ apt-install.sh <paquet>...
#
# L'instantané n'est servi qu'en HTTPS, et l'image de base n'a pas de certificats racine :
# ca-certificates est d'abord installé depuis la poche de publication de la distribution,
# figée depuis sa sortie. Les index sont dans les deux cas authentifiés par la clé de
# l'archive Ubuntu.
set -eu

case "${APT_SNAPSHOT:-}" in
  [0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]T[0-9][0-9][0-9][0-9][0-9][0-9]Z) ;;
  *)
    echo "APT_SNAPSHOT absent ou mal formé (attendu : AAAAMMJJTHHMMSSZ)" >&2
    exit 1
    ;;
esac
if [ "$#" -eq 0 ]; then
  echo "Aucun paquet demandé" >&2
  exit 1
fi

. /etc/os-release
if [ "${ID:-}" != ubuntu ]; then
  echo "Distribution non prise en charge : ${ID:-inconnue}" >&2
  exit 1
fi

export DEBIAN_FRONTEND=noninteractive
release_list=$(mktemp)
trap 'rm -f "$release_list"' EXIT
echo "deb [signed-by=/usr/share/keyrings/ubuntu-archive-keyring.gpg] http://archive.ubuntu.com/ubuntu ${VERSION_CODENAME} main" \
  > "$release_list"
release_only="-o Dir::Etc::SourceParts=/nonexistent -o Dir::Etc::SourceList=$release_list"

# shellcheck disable=SC2086 # options volontairement découpées
apt-get $release_only update
# shellcheck disable=SC2086
apt-get $release_only install -y --no-install-recommends ca-certificates

apt-get update --snapshot "$APT_SNAPSHOT"
apt-get install -y --no-install-recommends --snapshot "$APT_SNAPSHOT" "$@"
