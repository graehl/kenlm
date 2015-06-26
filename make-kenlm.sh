#The following optional packages are autodetected:
#libtcmalloc_minimal from google-perftools
#bzip2
#lzma aka xz
scriptdir=`dirname $0`
. $scriptdir/../usegcc.sh
cp $scriptdir/../user-config.jam ~
xmtext=${1:-$HOME/c/xmt-externals/FC12}
icusubdir=${2:-icu-55.1}
cd ${3:-$scriptdir}
boostminor=${4:-58}
boostsub=boost_${boostver:-1_${boostminor}_0}
sharedinc=$xmtext/../Shared/cpp
boostinc=$sharedinc/$boostsub/include
boostlib=$xmtext/libraries/$boostsub/lib
nplm=${nplm:-nplm}
#nplm=nplm01
nplminc=$sharedinc/$nplm/include/$nplm
nplmlib=$xmtext/libraries/$nplm/lib
eigeninc=$sharedinc/eigen-3.2.5
maxorders="5 7"
if false ; then
    links="shared static"
    linksufs="so a"
else
    links="static"
    linksufs="a"
fi
makekenlm() {
    local m=$1
    set -e
    [[ -d $boostinc ]]
    [[ -d $boostlib ]]
    if ! [[ -f lm/binary_format.hh ]] ; then
        echo run from kenlm source root
        exit 1
    fi
    if ! [[ $noclean ]] ; then
      rm -rf lib include
      find . -name bin -exec rm -rf {} \;
    fi
    icu=$xmtext/libraries/$icusubdir
    toolset=gcc
    if [[ $lwarch = Apple ]] ; then
        icu=/usr/local/Cellar/icu4c/55.1
        toolset=Darwin
    fi
    withicu="--with-icu=$icu"
    if [[ `uname` = Linux ]] ; then
        memcpyarg="cflags=-include cflags=util/glibc_memcpy.hh"
    fi
    marg="-s max-order=$m --max-kenlm-order=$m"
    archargs=
    for f in $archflags; do
        archargs+=" cxxflags=$f"
    done
    nplmarg="--with-nplm=$scriptdir/../$nplm/src"
    nplmpathsarg="cxxflags=-I$nplminc  linkflags=-L$nplmlib"
    # 'with-nplm' path doesn't work, or i don't understand. instead, edit lm/Jamfile?
    boostarg="cxxflags=-I$boostinc cxxflags=-I$eigeninc linkflags=-L$boostlib"
    warnarg="cxxflags=-Wno-unused-variable cxxflags=-Wno-deprecated-declarations cxxflags=-Wno-sign-compare cxxflags=-Wno-strict-overflow cxxflags=-Wno-reorder"
    #  ldflags=
    set -x
    for link in $links; do
        LD_LIBRARY_PATH=$nplmlib:$LD_LIBRARY_PATH ./bjam -d+2 $nplmarg $nplmpathsarg $marg link=$link variant=release debug-symbols=off $warnarg $memcpyarg $boostarg $withicu $archargs --with-toolset=$toolset -j10
    done
    set +x
}
installkenlm() {
    local m=$1
    set -e
    headers=$xmtext/../Shared/cpp/KenLM/include/KenLM
    mkdir -p $headers
    cp -a include/* $headers
    from=
    libfrom=$scriptdir/lib
    binfrom=$scriptdir/lm/bin/gcc-5/release/link-static/threading-multi/
    rm -f $binfrom/*.o
    mgram=$xmtext/libraries/KenLM/$m-gram
    libs=$mgram/lib
    bins=$mgram/bin
    mkdir -p $libs
    mkdir -p $bins
    set -x
    for s in $linksufs; do
        from=$libfrom/libkenlm.$s
        cp $from $libs/libKenLM.$s &&
        ln -sf libKenLM.$s $libs/libKenLM_debug.$s || echo no $from
    done
    cp $binfrom/* $bins
    set +x
    echo
    echo DONE $m-gram
}
makekenlm 7 && installkenlm 7
makekenlm 5 && installkenlm 5
