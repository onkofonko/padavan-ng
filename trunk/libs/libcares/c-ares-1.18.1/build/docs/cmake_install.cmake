# Install script for directory: /opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xDevelx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/share/man/man3/ares_cancel.3;/share/man/man3/ares_create_query.3;/share/man/man3/ares_destroy.3;/share/man/man3/ares_destroy_options.3;/share/man/man3/ares_dup.3;/share/man/man3/ares_expand_name.3;/share/man/man3/ares_expand_string.3;/share/man/man3/ares_fds.3;/share/man/man3/ares_free_data.3;/share/man/man3/ares_free_hostent.3;/share/man/man3/ares_free_string.3;/share/man/man3/ares_freeaddrinfo.3;/share/man/man3/ares_get_servers.3;/share/man/man3/ares_get_servers_ports.3;/share/man/man3/ares_getaddrinfo.3;/share/man/man3/ares_gethostbyaddr.3;/share/man/man3/ares_gethostbyname.3;/share/man/man3/ares_gethostbyname_file.3;/share/man/man3/ares_getnameinfo.3;/share/man/man3/ares_getsock.3;/share/man/man3/ares_inet_ntop.3;/share/man/man3/ares_inet_pton.3;/share/man/man3/ares_init.3;/share/man/man3/ares_init_options.3;/share/man/man3/ares_library_cleanup.3;/share/man/man3/ares_library_init.3;/share/man/man3/ares_library_init_android.3;/share/man/man3/ares_library_initialized.3;/share/man/man3/ares_mkquery.3;/share/man/man3/ares_parse_a_reply.3;/share/man/man3/ares_parse_aaaa_reply.3;/share/man/man3/ares_parse_caa_reply.3;/share/man/man3/ares_parse_mx_reply.3;/share/man/man3/ares_parse_naptr_reply.3;/share/man/man3/ares_parse_ns_reply.3;/share/man/man3/ares_parse_ptr_reply.3;/share/man/man3/ares_parse_soa_reply.3;/share/man/man3/ares_parse_srv_reply.3;/share/man/man3/ares_parse_txt_reply.3;/share/man/man3/ares_parse_uri_reply.3;/share/man/man3/ares_process.3;/share/man/man3/ares_query.3;/share/man/man3/ares_save_options.3;/share/man/man3/ares_search.3;/share/man/man3/ares_send.3;/share/man/man3/ares_set_local_dev.3;/share/man/man3/ares_set_local_ip4.3;/share/man/man3/ares_set_local_ip6.3;/share/man/man3/ares_set_servers.3;/share/man/man3/ares_set_servers_csv.3;/share/man/man3/ares_set_servers_ports.3;/share/man/man3/ares_set_servers_ports_csv.3;/share/man/man3/ares_set_socket_callback.3;/share/man/man3/ares_set_socket_configure_callback.3;/share/man/man3/ares_set_socket_functions.3;/share/man/man3/ares_set_sortlist.3;/share/man/man3/ares_strerror.3;/share/man/man3/ares_timeout.3;/share/man/man3/ares_version.3")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
file(INSTALL DESTINATION "/share/man/man3" TYPE FILE FILES
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_cancel.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_create_query.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_destroy.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_destroy_options.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_dup.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_expand_name.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_expand_string.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_fds.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_free_data.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_free_hostent.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_free_string.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_freeaddrinfo.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_get_servers.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_get_servers_ports.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_getaddrinfo.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_gethostbyaddr.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_gethostbyname.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_gethostbyname_file.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_getnameinfo.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_getsock.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_inet_ntop.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_inet_pton.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_init.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_init_options.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_library_cleanup.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_library_init.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_library_init_android.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_library_initialized.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_mkquery.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_parse_a_reply.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_parse_aaaa_reply.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_parse_caa_reply.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_parse_mx_reply.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_parse_naptr_reply.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_parse_ns_reply.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_parse_ptr_reply.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_parse_soa_reply.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_parse_srv_reply.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_parse_txt_reply.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_parse_uri_reply.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_process.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_query.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_save_options.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_search.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_send.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_set_local_dev.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_set_local_ip4.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_set_local_ip6.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_set_servers.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_set_servers_csv.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_set_servers_ports.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_set_servers_ports_csv.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_set_socket_callback.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_set_socket_configure_callback.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_set_socket_functions.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_set_sortlist.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_strerror.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_timeout.3"
    "/opt/padavan-ng/trunk/libs/libcares/c-ares-1.18.1/docs/ares_version.3"
    )
endif()

