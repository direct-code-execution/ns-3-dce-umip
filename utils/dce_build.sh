#!/bin/bash

cd `dirname $BASH_SOURCE`/../..

# build umip
git clone git://git.umip.org/umip.git
cd umip
autoreconf -i
CFLAGS="-fPIC -g" CXXFLAGS="-fPIC -g" LDFLAGS="-pie -g" ./configure --enable-vt --with-builtin-crypto
make
cp -f src/mip6d  ../ns-3-dce/build/bin_dce
cd ..


# mode ns-3-dev (FIXME)
cd ns-3-dev
sed "s/NS_ASSERT_MSG (nextHeaderPosition/\/\/NS_ASSERT_MSG (nextHeaderPosition/" src/internet/model/ipv6-l3-protocol.cc >a
mv a src/internet/model/ipv6-l3-protocol.cc
./waf
./waf install
cd ..

# mod ns-3-dce (FIXME)
#hg clone http://202.249.37.8/ical/ns-3-dce-patches/
cd ns-3-dce-patches
hg pull -u
cd ..
cd ns-3-dce
patch -p1 < ../ns-3-dce-patches/120410-dce-umip-support.patch
. ./utils/setenv.sh
./waf configure --prefix=`pwd`/../build --verbose --enable-kernel-stack=`pwd`/../ns-3-linux
./waf
./waf install
cd ..

# mod ns-3-linux (FIXME)
hg clone http://202.249.37.8/ical/ns-3-linux-patches/
cd ns-3-linux-patches
hg pull -u
cd ..
cd ns-3-linux
patch -p1 < ../ns-3-linux-patches/120410-linux-umip-support.patch
cd net-next-2.6
patch -p1 < ../kernel-dsmip6.patch
cd ..
make clean
rm -f config
make config
make
cd ..

# build umip-dsmip6
##wget ftp://ftp.linux-ipv6.org/pub/usagi/patch/mipv6/umip-0.4/daemon/tarball/mipv6-daemon-umip-0.4.tar.gz
wget http://repository.timesys.com/buildsources/m/mipv6-daemon-umip/mipv6-daemon-umip-0.4/mipv6-daemon-umip-0.4.tar.gz
wget http://software.nautilus6.org/packages/DSMIP/umip-dsmip-20080530.tar.bz2
wget http://software.nautilus6.org/packages/DSMIP/umip-dsmip-0.2-patches.tar.bz2
tar xfz mipv6-daemon-umip-0.4.tar.gz
tar xfj umip-dsmip-20080530.tar.bz2
tar xfj umip-dsmip-0.2-patches.tar.bz2
cd mipv6-daemon-umip-0.4
# first patch
patch -p1 < ../umip-dsmip-20080530/userland-dsmip-20080530/0001-upstream_fix_memset_in_bcache_alloc.patch
patch -p1 < ../umip-dsmip-20080530/userland-dsmip-20080530/0002-cleanup_trailing_spaces.patches
patch -p1 < ../umip-dsmip-20080530/userland-dsmip-20080530/0003-k_flag.patch
patch -p1 < ../umip-dsmip-20080530/userland-dsmip-20080530/0004-nepl.patch
patch -p1 < ../umip-dsmip-20080530/userland-dsmip-20080530/0005-conf_display.patch
patch -p1 < ../umip-dsmip-20080530/userland-dsmip-20080530/0006-dsmipv6.patch
patch -p1 < ../umip-dsmip-20080530/userland-dsmip-20080530/0007-remove_assert_0.patch
patch -p1 < ../umip-dsmip-20080530/userland-dsmip-20080530/0008-HomeAgentName.patch
patch -p1 < ../umip-dsmip-20080530/userland-dsmip-20080530/0009-dhcp_support.patch
patch -p1 < ../umip-dsmip-20080530/userland-dsmip-20080530/0010-remove_v4_mapped_addresses.patch
patch -p1 < ../umip-dsmip-20080530/userland-dsmip-20080530/0011-NAT-traversal.patch
patch -p1 < ../umip-dsmip-20080530/userland-dsmip-20080530/0012-NAT-UDP-encap.patch
patch -p1 < ../umip-dsmip-20080530/userland-dsmip-20080530/0013-using_nat_option.patch
# second patch
patch -p1 < ../umip-dsmip-0.2-patches/01-DSMIPv6-delete_addr_from_iface.patch
patch -p1 < ../umip-dsmip-0.2-patches/02-DSMIPv6-MR_xfrm_removal.patch
patch -p1 < ../umip-dsmip-0.2-patches/03-DSMIPv6-sit_makes_crash_OS.patch
patch -p1 < ../umip-dsmip-0.2-patches/04-DSMIPv6-HA_xfrm_removal.patch
patch -p1 < ../umip-dsmip-0.2-patches/05-DSMIPv6-handover64.patch
patch -p1 < ../umip-dsmip-0.2-patches/06-DSMIPv6-externalDhcp-handover44.patch

sed "s/#include <netinet\/in.h>/#include <stdio.h>\n#include <netinet\/in.h>/" libmissing/inet6_rth_init.c > a
mv a libmissing/inet6_rth_init.c
sed "s/#include <netinet\/in.h>/#include <stdio.h>\n#include <netinet\/in.h>/" libmissing/inet6_rth_getaddr.c > a
mv a libmissing/inet6_rth_getaddr.c

autoreconf -i
CFLAGS="-fPIC -g -I`pwd`/../ns-3-linux/net-next-2.6/include" CXXFLAGS="-fPIC -g" LDFLAGS="-pie -g" ./configure --enable-vt --with-builtin-crypto
make
cp -f src/mip6d ../ns-3-dce/build/bin_dce/mip6d.dsmip
cd ..

# build udhcpd
wget https://launchpad.net/ubuntu/+archive/primary/+files/udhcp_0.9.8cvs20050303.orig.tar.gz
tar xfz udhcp_0.9.8cvs20050303.orig.tar.gz
cd udhcp
CFLAGS="-fPIC -g" LDFLAGS=-pie make
cp -f udhcpd ../ns-3-dce/build/bin_dce/
cd ..

# build ns-3-dce-umip
cd ns-3-dce-umip
. ../ns-3-dce/utils/setenv.sh
cd ../ns-3-dce-umip
./waf configure --prefix=`pwd`/../build --verbose --enable-kernel-stack=`pwd`/../ns-3-linux
./waf
./waf install
echo Launch NS3UMIPTEST-DCE
./build/bin/ns3test-dce-umip --verbose


