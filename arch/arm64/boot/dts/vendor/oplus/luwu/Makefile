ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_LAHAINA) += \
        luwu-21005-lahaina-overlay.dtbo \
        luwu-21005-lahaina-v2.1-overlay.dtbo

luwu-21005-lahaina-overlay.dtbo-base := luwu-lahaina.dtb luwu-lahaina-v2.dtb luwu-lahaina-v2.1.dtb
luwu-21005-lahaina-v2.1-overlay.dtbo-base := luwu-lahaina.dtb luwu-lahaina-v2.dtb luwu-lahaina-v2.1.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
