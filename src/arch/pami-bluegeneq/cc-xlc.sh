XLC_TYPICAL_PRE=/soft/compilers/ibmcmp-may2012
XLC_PRE=$XLC_TYPICAL_PRE/vacpp/bg/12.1

XLC_TYPICAL_POST=bin/bg
XLC_POST=$XLC_TYPICAL_POST

# if no floor set, use typical floor path
if test -n "$BGQ_XLC_PRE"
then
  XLC_PRE=$BGQ_XLC_PRE
fi

XLC_F=$XLC_TYPICAL_PRE/xlf/bg/14.1
CMK_CC="$XLC_PRE/${XLC_POST}xlc_r -qcpluscmt -qhalt=e $BGQ_INC"
CMK_CXX="$XLC_PRE/${XLC_POST}xlC_r -qhalt=e $BGQ_INC"
CMK_LD="$CMK_CC"
CMK_LDXX="$CMK_CXX"
CMK_CF77="$XLC_F/${XLC_POST}xlf "
CMK_CF90="$XLC_F/${XLC_POST}xlf90  -qsuffix=f=f90" 
CMK_CF90_FIXED="$XLC_PRE/xlf/8.1/${XLC_POST}xlf90 " 
CMK_C_OPTIMIZE='-O3 -Q'
CMK_CXX_OPTIMIZE='-O3 -Q'
CMK_ENABLE_C11='-qlanglvl=extc1x'
CMK_ENABLE_CPP11='-qlanglvl=extended0x'

CMK_AR='ar cq'
CMK_NM='nm '
CMK_QT="aix"
#CMK_NATIVE_CC="/opt/ibmcmp/vacpp/bg/9.0/bin/xlc"
#CMK_NATIVE_CXX="/opt/ibmcmp/vacpp/bg/9.0/bin/xlC"
CMK_NATIVE_LD="$CMK_NATIVE_CC"
CMK_NATIVE_LDXX="$CMK_NATIVE_CXX"
CMK_RANLIB="ranlib"
CMK_F90LIBS="-L$XLC_F/lib -lxlf90 -lxlopt -lxl -lxlfmath"
