#
# Annapurna AL SoC support
#

# Annapurna Alpine drivers
device		al_ccu			# Alpine Cache Coherency Unit
device		al_nb_service		# Alpine North Bridge Service
device		al_iofic		# I/O Fabric Interrupt Controller
device		al_serdes		# Serializer/Deserializer
device		al_udma			# Universal DMA

# CPU frequency control
device		cpufreq

# Bus drivers
device		pci
device		al_pci			# Annapurna Alpine PCI-E
options 	PCI_HP			# PCI-Express native HotPlug
options 	PCI_IOV			# PCI SR-IOV support

# Block devices
device		ahci
device		scbus
device		da

# ATA/SCSI peripherals
device		cd			# CD
device		pass			# Passthrough device (direct ATA/SCSI access)

# NVM Express (NVMe) support
device		nvme			# base NVMe driver
options 	NVME_USE_NVD=0		# prefer the cam(4) based nda(4) driver
device		nvd			# expose NVMe namespaces as disks, depends on nvme

# GPIO / PINCTRL
device		gpio

# Crypto accelerators
device		armv8crypto		# ARMv8 OpenCrypto module

# Console
device		vt
device		kbdmux

# EVDEV support
device		evdev			# input event device support
options 	EVDEV_SUPPORT		# evdev support in legacy drivers
device		uinput			# install /dev/uinput cdev

# Serial (COM) ports
device		uart			# Generic UART driver
device		uart_ns8250		# ns8250-type UART driver

# PCI/PCI-X/PCIe Ethernet NICs that use iflib infrastructure
device		iflib
device		em			# Intel PRO/1000 Gigabit Ethernet Family
device		ix			# Intel 10Gb Ethernet Family

# Ethernet NICs
device		mdio
device		mii
device		miibus			# MII bus support
device		al_eth			# Annapurna Alpine Ethernet NIC

# Pseudo devices.
device		crypto			# core crypto support
device		loop			# Network loopback
device		ether			# Ethernet support
device		vlan			# 802.1Q VLAN support
device		tuntap			# Packet tunnel.
device		md			# Memory "disks"
device		gif			# IPv6 and IPv4 tunneling
device		firmware		# firmware assist module

# EXT_RESOURCES pseudo devices
options 	EXT_RESOURCES
device		clk
device		phy
device		hwreset
device		nvmem
device		regulator
device		syscon

# The `bpf' device enables the Berkeley Packet Filter.
# Be aware of the administrative consequences of enabling this!
# Note that 'bpf' is required for DHCP.
device		bpf		# Berkeley packet filter

# USB support
options 	USB_DEBUG		# enable debug msgs
options 	USB_HOST_ALIGN=64	# Align usb buffers to cache line size.
device		ohci			# OHCI USB interface
device		uhci			# UHCI USB interface
device		ehci			# EHCI USB interface (USB 2.0)
device		xhci			# XHCI USB interface (USB 3.0)
device		usb			# USB Bus (required)
device		ukbd			# Keyboard
device		umass			# Disks/Mass storage - Requires scbus and da

# Sound support
device		sound

options 	FDT
device		acpi

# HID support
options 	HID_DEBUG		# enable debug msgs
device		hid			# Generic HID support
