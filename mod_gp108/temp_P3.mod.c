#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x2b7122af, "dma_free_attrs" },
	{ 0xac7d8279, "pci_iounmap" },
	{ 0xd154c24b, "pci_release_regions" },
	{ 0x3a15cc20, "pci_disable_device" },
	{ 0x37a0cba, "kfree" },
	{ 0x92997ed8, "_printk" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x6e890bb9, "pci_unregister_driver" },
	{ 0xa7f2eff6, "kmalloc_caches" },
	{ 0xbbafc588, "__kmalloc_cache_noprof" },
	{ 0xd2e331ed, "pci_enable_device" },
	{ 0xdac49586, "pci_set_master" },
	{ 0x14876d73, "pci_request_regions" },
	{ 0xd4d15c48, "dma_set_mask" },
	{ 0x89dd6ec2, "dma_set_coherent_mask" },
	{ 0xbe0f8d9c, "_dev_warn" },
	{ 0x6199ae68, "pci_iomap" },
	{ 0xb998f4fe, "misc_register" },
	{ 0x1ecdd11a, "dma_alloc_attrs" },
	{ 0xc9ddfa4b, "pci_clear_master" },
	{ 0xc391f840, "_dev_err" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xdbcf7d05, "__pci_register_driver" },
	{ 0xf5ee4669, "misc_deregister" },
	{ 0x2f9b2b9a, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("pci:v000010DEd00001D01sv*sd*bc*sc*i*");
