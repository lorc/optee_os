global-incdirs-y += include

srcs-y += bget_alloc.c
cflags-remove-bget_alloc.c-y += -Wold-style-definition -Wredundant-decls
cflags-bget_alloc.c-y += -Wno-sign-compare -Wno-cast-align
ifeq ($(sm),core)
cflags-remove-bget_alloc.c-y += $(cflags_kasan)
endif
srcs-y += malloc.c

srcs-y += snprintf.c

srcs-y += stack_check.c
srcs-y += qsort.c
cflags-qsort.c-y += -Wno-inline
cflags-remove-qsort.c-y += -Wcast-align

srcs-y += strdup.c
srcs-y += strndup.c

subdirs-y += newlib
subdirs-$(arch_arm) += arch/$(ARCH)
