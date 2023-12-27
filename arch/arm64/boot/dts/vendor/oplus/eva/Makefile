ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_LAHAINA) += \
        eva-21147-lahaina-overlay.dtbo \
        eva-21147-lahaina-v2.1-overlay.dtbo

eva-21147-lahaina-overlay.dtbo-base := eva-lahaina.dtb eva-lahaina-v2.dtb eva-lahaina-v2.1.dtb
eva-21147-lahaina-v2.1-overlay.dtbo-base := eva-lahaina.dtb eva-lahaina-v2.dtb eva-lahaina-v2.1.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
