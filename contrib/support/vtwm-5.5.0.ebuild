# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI="3"

inherit xorg-2

DESCRIPTION="Last TWM descendant standing.  Implements a Virtual Desktop"
HOMEPAGE="http://www.vtwm.org/"
SRC_URI="http://sourceforge.net/projects/vtwm/files/${P}.tar.gz"

LICENSE="MIT"
SLOT="0"
KEYWORDS="~alpha ~amd64 ~ppc ~sparc ~x86"
IUSE="png rplay +xft +xinerama xpm +xrandr"

RDEPEND="x11-libs/libX11
	x11-libs/libICE
	x11-libs/libSM
	x11-libs/libXext
	x11-libs/libXmu
	x11-libs/libXt
	png? ( media-libs/libpng )
	rplay? ( media-sound/rplay )
	xft? ( x11-libs/libXft x11-libs/libXrender )
	xinerama? ( x11-libs/libXinerama )
	xpm? ( x11-libs/libXpm )
	xrandr? ( x11-libs/libXrandr )"
DEPEND="${RDEPEND}"

DOCS="doc/*"
