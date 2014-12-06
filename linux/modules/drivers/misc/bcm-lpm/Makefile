obj-$(CONFIG_BCM_BT_LPM) += bcm_bt_lpm.o

ifneq (,$(filter FE375CG ME572CL ME572C,$(TARGET_PROJECT)))
    KBUILD_CFLAGS += -DFOR_2076
endif
