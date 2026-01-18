PCIe acquisition driver study project

Overview
- baseline/ contains the unoptimized driver from the docs (hard IRQ work, per-transfer map/unmap)
- optimized/ contains the improved driver (threaded IRQ, SG DMA setup, PM hooks)
- tools/ has a small user-space reader for quick smoke tests

Build (inside your guest or on the target machine)
- Baseline: `make -C /home/linger/Linux/Pcie_Project/baseline`
- Optimized: `make -C /home/linger/Linux/Pcie_Project/optimized`
- User tool: `make -C /home/linger/Linux/Pcie_Project/tools`

QEMU device options
- `-device edu` (vendor 0x1234, device 0x11e8)
- `-device pci-testdev,membar=65536` (use `lspci -nn` to confirm IDs)

If the device IDs do not match the driver table, bind manually:
- `echo "<vendor> <device>" | sudo tee /sys/bus/pci/drivers/pcie_acq/new_id`

Load and test
- Baseline: `sudo insmod /home/linger/Linux/Pcie_Project/baseline/pcie_acq_baseline.ko`
- Optimized: `sudo insmod /home/linger/Linux/Pcie_Project/optimized/pcie_acq_optimized.ko`
- Reader: `sudo /home/linger/Linux/Pcie_Project/tools/acq_read -n 5`

Notes
- When using the QEMU `pcie-acq` device model, data-ready IRQs come from the virtual device.
