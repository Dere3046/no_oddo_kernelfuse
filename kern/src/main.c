// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/arm-smccc.h>

#include "core.h"
#include "hk.h"
#include "hk_inline.h"

#define ARB_SMC_FN 0x4200011eUL

static unsigned long __nocfi arb_resolve(const char *name)
{
	if (kallrecon_klp)
		return kallrecon_klp(name);
	return 0;
}

static struct hk_inline hk_sysfs;
static struct hk_inline hk_api;
static struct hk_inline hk_smc;

__nocfi ssize_t arb_sysfs_wrapper(struct kobject *kobj,
				  struct attribute *attr,
				  const char *buf, size_t count)
{
	pr_info("[arb_block] block sysfs update_arb store\n");
	return count;
}

__nocfi int arb_api_wrapper(void)
{
	pr_info("[arb_block] block qcom_scm_update_rollback_version\n");
	return 0;
}

__nocfi void arb_smc_wrapper(unsigned long a0, unsigned long a1,
			     unsigned long a2, unsigned long a3,
			     unsigned long a4, unsigned long a5,
			     unsigned long a6, unsigned long a7,
			     struct arm_smccc_res *res,
			     struct arm_smccc_quirk *quirk)
{
	if (a0 == ARB_SMC_FN) {
		pr_info("[arb_block] block SMC rollback fn=0x%lx\n", a0);
		if (res) {
			res->a0 = 0;
			res->a1 = 0;
			res->a2 = 0;
			res->a3 = 0;
		}
		return;
	}

	((void (*)(unsigned long, unsigned long, unsigned long,
		   unsigned long, unsigned long, unsigned long,
		   unsigned long, unsigned long,
		   struct arm_smccc_res *, struct arm_smccc_quirk *))
	 hk_smc.orig)(a0, a1, a2, a3, a4, a5, a6, a7, res, quirk);
}

static int __init arb_block_init(void)
{
	struct hk_cfg cfg = {
		.resolve = arb_resolve,
	};
	int ret;

	find_kallsyms_base();
	if (!klnum_val || !kallrecon_klp) {
		pr_warn("[arb_block] kallsyms recovery failed\n");
		return -ENODATA;
	}
	pr_info("[arb_block] kallsyms ok syms=%u\n", klnum_val);
	pr_info("[arb_block] __arm_smccc_smc=0x%lx\n",
		arb_resolve("__arm_smccc_smc"));
	if (!arb_resolve("__arm_smccc_smc")) {
		pr_warn("[arb_block] __arm_smccc_smc not found\n");
		return -ENODATA;
	}

	ret = hk_init(&cfg);
	if (ret)
		return ret;

	ret = hk_inline_hook(&hk_sysfs, "update_arb_store",
			     "arb_sysfs_wrapper");
	if (ret)
		pr_warn("[arb_block] update_arb_store not hooked: %d\n", ret);
	else
		pr_info("[arb_block] hooked update_arb_store\n");

	ret = hk_inline_hook(&hk_api, "qcom_scm_update_rollback_version",
			     "arb_api_wrapper");
	if (ret)
		pr_warn("[arb_block] qcom_scm_update_rollback_version not hooked: %d\n", ret);
	else
		pr_info("[arb_block] hooked qcom_scm_update_rollback_version\n");

	ret = hk_inline_hook(&hk_smc, "__arm_smccc_smc", "arb_smc_wrapper");
	if (ret) {
		pr_warn("[arb_block] hook __arm_smccc_smc failed %d\n", ret);
		hk_inline_unhook(&hk_api);
		hk_inline_unhook(&hk_sysfs);
		hk_exit();
		return ret;
	}
	pr_info("[arb_block] hooked __arm_smccc_smc\n");

	pr_info("[arb_block] loaded\n");
	return 0;
}

static void __exit arb_block_exit(void)
{
	hk_inline_unhook(&hk_smc);
	hk_inline_unhook(&hk_api);
	hk_inline_unhook(&hk_sysfs);
	hk_exit();
	pr_info("[arb_block] unloaded\n");
}

module_init(arb_block_init);
module_exit(arb_block_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("OnePlus ARB rollback block module");
MODULE_AUTHOR("dere3046");
