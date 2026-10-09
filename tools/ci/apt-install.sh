#!/bin/sh
# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2
#
# Installe des paquets Ubuntu depuis un instantané daté de l'archive (snapshot.ubuntu.com),
# pour que deux exécutions installent les mêmes versions.
#
# Usage : APT_SNAPSHOT=AAAAMMJJTHHMMSSZ apt-install.sh <paquet>...
#
# Les index (/var/lib/apt/lists) et les paquets téléchargés (/var/cache/apt/archives) sont
# conservés pour être mis en cache par la CI. Si un cache de cet instantané a été restauré,
# l'installation se fait sans réseau ; sinon, les index et paquets sont téléchargés, avec
# plusieurs tentatives, et tout index manquant fait échouer le script explicitement.
#
# L'instantané n'est servi qu'en HTTPS, et l'image de base n'a pas de certificats racine :
# sans cache, ca-certificates est d'abord installé depuis la poche de publication de la
# distribution, figée depuis sa sortie. Les index sont dans les deux cas authentifiés par la
# clé de l'archive Ubuntu.
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

# shellcheck disable=SC1091 # fichier du système
. /etc/os-release
if [ "${ID:-}" != ubuntu ]; then
  echo "Distribution non prise en charge : ${ID:-inconnue}" >&2
  exit 1
fi

export DEBIAN_FRONTEND=noninteractive
# L'image efface les paquets après installation : on les garde pour le cache.
rm -f /etc/apt/apt.conf.d/docker-clean
retries="-o Acquire::Retries=5"
marker=/var/lib/apt/lists/clepsydre-snapshot

if [ -f "$marker" ] && [ "$(cat "$marker")" = "$APT_SNAPSHOT" ]; then
  echo "Index de l'instantané $APT_SNAPSHOT restaurés : installation depuis le cache."
  # Sans réseau, apt ne peut rien télécharger : un paquet absent du cache fait échouer
  # l'installation plutôt que de contacter le service.
  # shellcheck disable=SC2086 # options volontairement découpées
  apt-get install -y --no-install-recommends --snapshot "$APT_SNAPSHOT" \
    -o Acquire::http::Proxy=http://127.0.0.1:9 -o Acquire::https::Proxy=http://127.0.0.1:9 "$@" || {
    echo "Cache incomplet pour ces paquets : téléchargement depuis l'instantané." >&2
    rm -f "$marker"
  }
  [ -f "$marker" ] && exit 0
fi

release_list=$(mktemp)
trap 'rm -f "$release_list"' EXIT
echo "deb [signed-by=/usr/share/keyrings/ubuntu-archive-keyring.gpg] http://archive.ubuntu.com/ubuntu ${VERSION_CODENAME} main" \
  > "$release_list"
release_only="-o Dir::Etc::SourceParts=/nonexistent -o Dir::Etc::SourceList=$release_list"

# shellcheck disable=SC2086
if ! apt-get $retries $release_only update --error-on=any; then
  echo "Archive Ubuntu injoignable pour installer ca-certificates, et aucun cache valide : abandon." >&2
  exit 1
fi
# shellcheck disable=SC2086
apt-get $retries $release_only install -y --no-install-recommends ca-certificates

# shellcheck disable=SC2086
if ! apt-get $retries update --error-on=any --snapshot "$APT_SNAPSHOT"; then
  echo "Instantané $APT_SNAPSHOT injoignable (snapshot.ubuntu.com) et aucun cache valide : abandon." >&2
  exit 1
fi
# shellcheck disable=SC2086
apt-get $retries install -y --no-install-recommends --snapshot "$APT_SNAPSHOT" "$@"
echo "$APT_SNAPSHOT" > "$marker"
