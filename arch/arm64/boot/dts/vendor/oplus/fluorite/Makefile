ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_LAHAINA) += \
        fluorite-21051-lahaina-overlay.dtbo \
		fluorite-2105A-lahaina-overlay.dtbo

fluorite-21051-lahaina-overlay.dtbo-base := fluorite-lahaina.dtb fluorite-lahaina-v2.dtb fluorite-lahaina-v2.1.dtb
fluorite-2105A-lahaina-overlay.dtbo-base := fluorite-lahaina.dtb fluorite-lahaina-v2.dtb fluorite-lahaina-v2.1.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
