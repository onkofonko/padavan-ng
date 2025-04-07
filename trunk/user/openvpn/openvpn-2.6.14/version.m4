dnl define the OpenVPN version
define([PRODUCT_NAME], [OpenVPN])
define([PRODUCT_TARNAME], [openvpn])
define([PRODUCT_VERSION_MAJOR], [2])
define([PRODUCT_VERSION_MINOR], [6])
<<<<<<<< HEAD:trunk/user/openvpn/openvpn-2.6.13/version.m4
define([PRODUCT_VERSION_PATCH], [.13])
========
define([PRODUCT_VERSION_PATCH], [.14])
>>>>>>>> upstream/master:trunk/user/openvpn/openvpn-2.6.14/version.m4
m4_append([PRODUCT_VERSION], [PRODUCT_VERSION_MAJOR])
m4_append([PRODUCT_VERSION], [PRODUCT_VERSION_MINOR], [[.]])
m4_append([PRODUCT_VERSION], [PRODUCT_VERSION_PATCH], [[]])
define([PRODUCT_BUGREPORT], [openvpn-users@lists.sourceforge.net])
<<<<<<<< HEAD:trunk/user/openvpn/openvpn-2.6.13/version.m4
define([PRODUCT_VERSION_RESOURCE], [2,6,13,0])
========
define([PRODUCT_VERSION_RESOURCE], [2,6,14,0])
>>>>>>>> upstream/master:trunk/user/openvpn/openvpn-2.6.14/version.m4
dnl define the TAP version
define([PRODUCT_TAP_WIN_COMPONENT_ID], [tap0901])
define([PRODUCT_TAP_WIN_MIN_MAJOR], [9])
define([PRODUCT_TAP_WIN_MIN_MINOR], [9])
