#
# Makefile for the touchscreen drivers.
#

ccflags-y += -Wall

# Each configuration option enables a list of files.
oplus_bsp_tp_ilitek7807s-y                   += ili7807s_qcom.o \
                                                ili7807s_ic.o \
                                                ili7807s_mp.o \
                                                ili7807s_fw.o \
                                                ili7807s_node.o
obj-$(CONFIG_TOUCHPANEL_ILITEK_ILITEK7807S) += oplus_bsp_tp_ilitek7807s.o
#endif
