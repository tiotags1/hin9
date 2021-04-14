# Copyright 1999-2021 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=7

inherit git-r3

DESCRIPTION="hinsightd is a http/1.1 webserver"
HOMEPAGE="https://gitlab.com/tiotags/hin9"
EGIT_REPO_URI="https://gitlab.com/tiotags/hin9.git"
LICENSE=""
SLOT="0"
KEYWORDS=""
IUSE="" #+openssl

RDEPEND="
sys-libs/liburing
dev-lang/lua
sys-libs/zlib
dev-libs/openssl
"

DEPEND="${RDEPEND}"

# Build-time dependencies that are executed during the emerge process, and
# only need to be present in the native build system (CBUILD). Example:
BDEPEND="dev-util/ninja"

#src_configure() {
#}

src_compile() {
  set -e
  cd build
  ninja
}

src_install() {
  set -e
  newbin ${S}/build/hin9 hinsightd
  newinitd ${S}/external/initd.sh hinsightd

  insinto /etc/hinsightd
  newins ${S}/workdir/main.lua hinsightd.lua

  keepdir /var/www/localhost/htdocs
# simple index file
}


