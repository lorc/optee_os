srcs-y += assert.c
#srcs-y += bget_kmalloc.c

#cflags-remove-bget_kmalloc.c-y += -Wold-style-definition -Wredundant-decls
#cflags-bget_kmalloc.c-y += -Wno-sign-compare -Wno-cast-align
#cflags-remove-bget_kmalloc.c-y += $(cflags_kasan)

srcs-y += console.c
srcs-$(CFG_DT) += dt.c
srcs-y += msg_param.c
srcs-y += tee_ta_manager.c
srcs-y += tee_misc.c
srcs-y += panic.c
srcs-y += handle.c
srcs-y += interrupt.c
srcs-$(CFG_CORE_SANITIZE_UNDEFINED) += ubsan.c
srcs-$(CFG_CORE_SANITIZE_KADDRESS) += asan.c
cflags-remove-asan.c-y += $(cflags_kasan)
srcs-y += refcount.c
