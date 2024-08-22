vendor := $(srctree)/$(src)
ifeq ($(strip $(CONFIG_CHG_FOR_OP9_PRO)), y)
        subdir-y += oplus/lemonadev
else ifeq ($(strip $(CONFIG_OPLUS_CHG_OP9RT_PMIC_VOOCPHY)), y)
        subdir-y += oplus/martini
else ifeq ($(strip $(CONFIG_ARCH_YUPIK)), y)
        subdir-y += oplus/walle
        subdir-y += oplus/libai
        subdir-y += oplus/lantian
        subdir-y += oplus/lunaa
        subdir-y += oplus/taylor
        subdir-y += oplus/garbo
        subdir-y += oplus/ziti
else
	subdir-y += oplus/cupid
	subdir-y += oplus/peacock
	subdir-y += oplus/fussi
	subdir-y += oplus/luwu
	subdir-y += oplus/eva
	subdir-y += oplus/fluorite
endif
