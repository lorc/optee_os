include core/arch/arm/kernel/link.mk

# Generate .srec file from binary, not from ELF, because TEE
# with paging enabled relies on layout of generated binary
all: $(link-out-dir)/tee.srec
cleanfiles += $(link-out-dir)/tee.srec
$(link-out-dir)/tee.srec: $(link-out-dir)/tee-whole_v2.bin
	@$(cmd-echo-silent) '  GEN     $@'
	$(q)$(OBJCOPYcore) -I binary -O srec $< $@
