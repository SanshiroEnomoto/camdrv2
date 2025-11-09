TARGET = camdrv

obj-m = $(TARGET).o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)


$(TARGET).ko: $(TARGET).c $(TARGET).h
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean


install: $(TARGET).ko
	@if [ ! -f $(TARGET).ko ]; then \
		echo "Error: $(TARGET).ko not found. Build failed."; \
		exit 1; \
	fi
	@if [ -d /etc/udev/rules.d ]; then \
		cp 99-camdrv.rules /etc/udev/rules.d; \
		echo "Installed udev rules to /etc/udev/rules.d/99-camdrv.rules"; \
	fi
	@if lsmod | grep -q "^$(TARGET) "; then \
		echo "$(TARGET) is already loaded"; \
	else \
		if insmod $(TARGET).ko 2>/dev/null; then \
			echo "Loaded $(TARGET).ko"; \
		else \
			echo "Warning: Failed to load $(TARGET).ko (device may not be connected)"; \
		fi; \
	fi
	@echo "Installation completed"


uninstall:
	-rmmod $(TARGET)
	@if [ -f /etc/udev/rules.d/99-camdrv.rules ]; then \
		rm -f /etc/udev/rules.d/99-camdrv.rules; \
		echo "Removed udev rules"; \
	fi
	@echo "Uninstalled $(TARGET)"
