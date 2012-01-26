LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

WOCKY_BUILT_SOURCES := \
	wocky/Android.mk

wocky-configure-real:
	cd $(WOCKY_TOP) ; \
	CC="$(CONFIGURE_CC)" \
	CFLAGS="$(CONFIGURE_CFLAGS)" \
	LD=$(TARGET_LD) \
	LDFLAGS="$(CONFIGURE_LDFLAGS)" \
	CPP=$(CONFIGURE_CPP) \
	CPPFLAGS="$(CONFIGURE_CPPFLAGS)" \
	PKG_CONFIG_LIBDIR="$(CONFIGURE_PKG_CONFIG_LIBDIR)" \
	PKG_CONFIG_TOP_BUILD_DIR=$(PKG_CONFIG_TOP_BUILD_DIR) \
	$(WOCKY_TOP)/$(CONFIGURE) --host=arm-linux-androideabi \
		--disable-Werror && \
	for file in $(WOCKY_BUILT_SOURCES); do \
		rm -f $$file && \
		make -C $$(dirname $$file) $$(basename $$file) ; \
	done

wocky-configure: wocky-configure-real

.PHONY: wocky-configure

CONFIGURE_TARGETS += wocky-configure

#include all the subdirs...
-include $(WOCKY_TOP)/wocky/Android.mk
