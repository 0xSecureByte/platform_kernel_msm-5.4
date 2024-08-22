ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_LAHAINA) += \
        porsche-21617-lahaina-overlay.dtbo \
        porsche-21617-lahaina-v2.1-overlay.dtbo

porsche-21617-lahaina-overlay.dtbo-base := porsche-lahaina.dtb porsche-lahaina-v2.dtb porsche-lahaina-v2.1.dtb
porsche-21617-lahaina-v2.1-overlay.dtbo-base := porsche-lahaina.dtb porsche-lahaina-v2.dtb porsche-lahaina-v2.1.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
