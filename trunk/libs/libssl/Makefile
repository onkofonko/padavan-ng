# https://openssl-library.org/source
# https://www.ccadb.org/resources
# https://wiki.mozilla.org/CA/Included_Certificates

ifneq (,$(filter y,$(QUIC_ENABLED) $(CONFIG_FIRMWARE_INCLUDE_OPENSSL_35)))
SSL_VER = 3.5
else ifeq ($(CONFIG_FIRMWARE_INCLUDE_OPENSSL_11),y)
SSL_VER = 1.1
else ifeq ($(CONFIG_FIRMWARE_INCLUDE_OPENSSL_10),y)
SSL_VER = 1.0
else
SSL_VER = 3.0
endif

all:
	$(MAKE) -C $(SSL_VER) all

%:
	$(MAKE) -C $(SSL_VER) $@

clean:
	for i in `ls -d *`; do \
		if [ -d $$i ]; then \
			$(MAKE) -C $$i clean; \
		fi; \
	done

romfs:
	$(MAKE) -C $(SSL_VER) romfs
	$(ROMFSINST) /etc_ro/ca-certificates.crt
	$(ROMFSINST) /etc_ro/openssl.cnf
