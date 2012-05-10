#!/usr/bin/make -f

CERT_DIR := ./certs
CA_KEY   := $(CERT_DIR)/ca-0-key.pem
CA_DIR   := $(CERT_DIR)/cas
CRL_DIR  := $(CERT_DIR)/crl
CERTTOOL := $(shell which certtool)
TARDIS   := $(shell which datefudge)
GENKEY   := $(CERTTOOL) --generate-privkey --outfile
CA_CERT  := $(CERT_DIR)/ca-0-cert.pem
CA_CERT2 := $(CERT_DIR)/ca-1-cert.pem
CA_CERT3 := $(CERT_DIR)/ca-2-cert.pem
CRTS     := new exp ca-0 ca-1 ca-2 rev ss tls unknown wild badwild
CERTS    := $(foreach name,$(CRTS),certs/$(name)-cert.pem)
KEYS     := $(foreach name,$(CRTS),certs/$(name)-key.pem)
REV_CERT := $(filter %rev-cert.pem, $(CERTS))
CRL      := $(CERT_DIR)/ca-0-crl.pem
CAS      := $(CA_CERT) $(CA_CERT2) $(CA_CERT3)
DAY      := 86400
YEAR     := 365 * $(DAY)
TIMEWARP := 28 * $(YEAR)
FUTURE   := $(shell date +'%Y-%m-%d %H:%M' -d @$$(($$(date +%s) + $(TIMEWARP))))
PAST     := $(shell date +'%Y-%m-%d %H:%M' -d @$$(($$(date +%s) - $(DAY) * 7)))
FILTER_BITS := grep -v '^[	 ]\+\([a-f0-9]\{2\}:\)\+[a-f0-9]\{2\}$$'
check_bin = $(or $(shell test -x "$(1)" && echo Y), $(error Need $(2) to $(3)))

.PHONY: certs targets
.PRECIOUS: $(KEYS) $(CAS)

help:
	@echo "-----------------------------------------------------------"
	@echo ./Certificates.mk certs 
	@echo Should recreate any missing certificates, crls or CAs
	@echo Removing a certificate and recreating it will also
	@echo recreate a private key for it if one is missing.
	@echo Each certificate has a .cfg file that determines it contents,
	@echo if you need to add or alter a certificate, start with its cfg
	@echo "-----------------------------------------------------------"
	@echo Known certificates: 
	@echo "[ $(CERTS) ]"

certs: $(CERTS) $(CRL) $(CAS)
certs: $(CRL_DIR)/ca-0-crl.pem                      
certs: $(foreach x,0 1 2,$(CA_DIR)/ca-$(x)-cert.pem)

# x509 certificates:
%-key.pem:
	@echo $(call check_bin,$(CERTTOOL),certtool,rebuild $@)
	$(GENKEY) $@

%/ss-cert.pem: %/ss-key.pem %/ss-cert.cfg
	@echo $(call check_bin,$(CERTTOOL),certtool,rebuild $@)
	$(CERTTOOL) --generate-self-signed            \
	            --load-privkey $<                 \
	            --template     $(basename $@).cfg \
	            --outfile      $@ 2>&1 | $(FILTER_BITS)

%/unknown-ca-cert.pem: %/unknown-ca-key.pem %/unknown-ca-cert.cfg
	@echo $(call check_bin,$(CERTTOOL),certtool,rebuild $@)
	$(CERTTOOL) --generate-self-signed            \
	            --load-privkey $<                 \
	            --template     $(basename $@).cfg \
	            --outfile      $@ 2>&1 | $(FILTER_BITS)

certs/ca-%-cert.pem $(CERT_DIR)/ca-%-cert.pem: $(CERT_DIR)/ca-%-key.pem $(CERT_DIR)/ca-%-cert.cfg
	@echo $(call check_bin,$(CERTTOOL),certtool,rebuild $@)
	$(CERTTOOL) --generate-self-signed            \
	            --load-privkey $<                 \
	            --template     $(basename $@).cfg \
	            --outfile      $@ 2>&1 | $(FILTER_BITS)

%/rev-cert.pem: export CERTCMD = $(CERTTOOL)
%/tls-cert.pem: export CERTCMD = $(CERTTOOL)
%/new-cert.pem: export CERTCMD = $(TARDIS) "$(FUTURE)" $(CERTTOOL)
%/exp-cert.pem: export CERTCMD = $(TARDIS) "$(PAST)"   $(CERTTOOL)
%/wild-cert.pem: export CERTCMD = $(CERTTOOL)
%/badwild-cert.pem: export CERTCMD = $(CERTTOOL)

$(NEW_CERT) $(EXP_CERT) certs/exp-cert.pem certs/new-cert.pem: NEED_TIME = 1

%/unknown-cert.pem: export CERTCMD = $(CERTTOOL)
%/unknown-cert.pem: export CA_CERT = $*/unknown-ca-cert.pem
%/unknown-cert.pem: export CA_KEY  = $*/unknown-ca-key.pem

%/unknown-cert.pem: %/unknown-key.pem %/unknown-cert.cfg %/unknown-ca-cert.pem
	@echo $(call check_bin,$(CERTTOOL),certtool,rebuild $@)
	$(CERTTOOL) --generate-certificate                       \
                --load-ca-certificate $*/unknown-ca-cert.pem \
	            --load-ca-privkey     $*/unknown-ca-key.pem  \
	            --load-privkey        $*/unknown-key.pem     \
	            --template            $*/unknown-cert.cfg    \
	            --outfile             $@ 2>&1 | $(FILTER_BITS)

%-cert.pem: %-key.pem %-cert.cfg $(CA_CERT)
	@echo "CERTIFICATE $@ ($(CERTCMD)): $^"
	@echo $(call check_bin,$(CERTTOOL),certtool,rebuild $@)
	@echo $(if $(NEED_TIME),$(call check_bin,$(TARDIS),datefudge,rebuild $@))
	$(CERTCMD)  --generate-certificate                   \
                --load-ca-certificate $(CA_CERT)         \
	            --load-ca-privkey     $(CA_KEY)	         \
	            --load-privkey        $<                 \
	            --template            $*-cert.cfg        \
	            --outfile             $@ 2>&1 | $(FILTER_BITS)

$(CRL): $(REV_CERT) $(CA_CERT) $(CA_KEY)
	@echo $(call check_bin,$(CERTTOOL),certtool,rebuild $@)
	$(CERTTOOL) --generate-crl        \
	            --template $(basename $@).cfg     \
	            --load-ca-privkey     $(CA_KEY)   \
	            --load-ca-certificate $(CA_CERT)  \
	            --load-certificate    $(REV_CERT) \
                --outfile             $@ 2>&1 | $(FILTER_BITS)

$(CA_DIR) $(CRL_DIR):
	@mkdir -p $@

$(CA_DIR)/%.pem: $(CERT_DIR)/%.pem
	@cp -av $< $@

$(CRL_DIR)/%.pem: $(CERT_DIR)/%.pem
	@cp -av $< $@
