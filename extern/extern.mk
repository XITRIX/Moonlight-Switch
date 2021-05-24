mkfile_path	:=	$(abspath $(lastword $(MAKEFILE_LIST)))

BOREALIS_PATH :=	$(EXTERN_PATH)/borealis
include $(TOPDIR)/$(BOREALIS_PATH)/library/borealis.mk
