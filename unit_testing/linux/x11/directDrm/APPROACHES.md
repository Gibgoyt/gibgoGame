 APPROACH 1: Fix Direct /dev/mem Access (FASTEST - ~0.1μs)

Your code is 99% correct - just needs a small fix for the mmap offset calculation.

The Problem:
// WRONG: Using physical address as file offset
mmap(NULL, size, PROT_RW, MAP_SHARED, mem_fd, 0x4D000000);  // FAILS

The Fix:
// CORRECT: Physical address AS the offset with /dev/mem
mmap(NULL, size, PROT_RW, MAP_SHARED, mem_fd, bar0_address & PAGE_MASK);  // WORKS

Requirements:
- Run as root: sudo ./triangle_hardware
- May need: echo 0 > /proc/sys/kernel/dma/restricted (disable DMA restrictions)
- May need: Add iommu=off to kernel boot parameters

APPROACH 2: VFIO Userspace Driver (VERY FAST - ~1μs)

Modern method for userspace hardware access with IOMMU support.

// Bind GPU to vfio-pci driver, then access via VFIO interface
int vfio_fd = open("/dev/vfio/vfio", O_RDWR);
// Map GPU BAR through VFIO interface - bypasses normal kernel driver

Performance: Nearly as fast as direct access, but with memory protection
Complexity: Medium - need to unbind GPU from nvidia driver first

APPROACH 3: Custom Kernel Module (ULTIMATE - ~0.05μs)

Write a minimal kernel module that exports GPU registers to userspace.

// In kernel module: Export GPU registers via custom device
static int gpu_direct_mmap(struct file *file, struct vm_area_struct *vma) {
    return io_remap_pfn_range(vma, vma->vm_start, gpu_bar0_pfn,
                             vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

Performance: Absolute maximum - direct hardware register access
Complexity: High - need to compile kernel module

