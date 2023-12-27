ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_LAHAINA) += \
        lahaina-peacock-20085-overlay.dtbo \
        lahaina-peacock-20085-v2.1-overlay.dtbo

lahaina-peacock-20085-overlay.dtbo-base := lahaina-peacock.dtb lahaina-peacock-v2.dtb lahaina-peacock-v2.1.dtb
lahaina-peacock-20085-v2.1-overlay.dtbo-base := lahaina-peacock.dtb lahaina-peacock-v2.dtb lahaina-peacock-v2.1.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
