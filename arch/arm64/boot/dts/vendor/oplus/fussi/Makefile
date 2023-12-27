ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_LAHAINA) += \
        fussi-20031-lahaina-overlay.dtbo \
        fussi-20031-lahaina-v2.1-overlay.dtbo

fussi-20031-lahaina-overlay.dtbo-base := fussi-lahaina.dtb fussi-lahaina-v2.dtb fussi-lahaina-v2.1.dtb
fussi-20031-lahaina-v2.1-overlay.dtbo-base := fussi-lahaina.dtb fussi-lahaina-v2.dtb fussi-lahaina-v2.1.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
