ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_LAHAINA) += \
		yupik-21071-overlay.dtbo \
		yupik-21075-overlay.dtbo

yupik-21071-overlay.dtbo-base := yupik.dtb
yupik-21075-overlay.dtbo-base := yupik.dtb

endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files    := *.dtb *.dtbo
