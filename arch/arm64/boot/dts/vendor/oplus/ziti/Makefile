ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_LAHAINA) += \
		yupik-22813-overlay.dtbo \
		yupik-22867-overlay.dtbo

yupik-22813-overlay.dtbo-base := yupik.dtb
yupik-22867-overlay.dtbo-base := yupik.dtb

endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files    := *.dtb *.dtbo
