ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_LAHAINA) += \
        cupid-20627-lahaina-overlay.dtbo \
        cupid-20627-lahaina-v2.1-overlay.dtbo

cupid-20627-lahaina-overlay.dtbo-base := cupid-lahaina.dtb cupid-lahaina-v2.dtb cupid-lahaina-v2.1.dtb
cupid-20627-lahaina-v2.1-overlay.dtbo-base := cupid-lahaina.dtb cupid-lahaina-v2.dtb cupid-lahaina-v2.1.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
