mkfile_path	:=	$(abspath $(lastword $(MAKEFILE_LIST)))

BOREALIS_PATH :=	$(EXTERN_PATH)/borealis
include $(TOPDIR)/$(BOREALIS_PATH)/library/borealis.mk

SOURCES		:=	$(SOURCES) \
				$(EXTERN_PATH)/moonlight-common-c/src \
				$(EXTERN_PATH)/moonlight-common-c/enet \
				$(EXTERN_PATH)/moonlight-common-c/reedsolomon 

INCLUDES	:=	$(INCLUDES) \
				$(EXTERN_PATH)/moonlight-common-c/src \
				$(EXTERN_PATH)/moonlight-common-c/enet/include \
				$(EXTERN_PATH)/moonlight-common-c/reedsolomon \
                $(EXTERN_PATH)/zeroconf

DEFINES := $(DEFINES) -DUSE_MBEDTLS
