/*
 * sun8i THS clock driver
 *
 * Copyright (C) 2015 Josef Gajdusek
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#define SUN8I_H3_THS_CLK_ENABLE				31
#define SUN8I_H3_THS_CLK_DIVIDER_SHIFT		0
#define SUN8I_H3_THS_CLK_DIVIDER_WIDTH		2

static DEFINE_SPINLOCK(sun8i_h3_ths_clk_lock);

static const struct clk_div_table sun8i_h3_ths_clk_table[] __initconst = {
	{ .val = 0, .div = 1 },
	{ .val = 1, .div = 2 },
	{ .val = 2, .div = 4 },
	{ .val = 3, .div = 6 },
	{ } /* sentinel */
};

static void __init sun8i_h3_ths_clk_setup(struct device_node *node)
{
	struct clk *clk;
	struct clk_gate *gate;
	struct clk_divider *div;
	const char *parent;
	const char *clk_name = node->name;
	void __iomem *reg;
	int err;

	reg = of_io_request_and_map(node, 0, of_node_full_name(node));

	if (IS_ERR(reg))
		return;

	gate = kzalloc(sizeof(*gate), GFP_KERNEL);
	if (!gate)
		goto err_unmap;

	div = kzalloc(sizeof(*gate), GFP_KERNEL);
	if (!div)
		goto err_gate_free;

	of_property_read_string(node, "clock-output-names", &clk_name);
	parent = of_clk_get_parent_name(node, 0);

	gate->reg = reg;
	gate->bit_idx = SUN8I_H3_THS_CLK_ENABLE;
	gate->lock = &sun8i_h3_ths_clk_lock;

	div->reg = reg;
	div->shift = SUN8I_H3_THS_CLK_DIVIDER_SHIFT;
	div->width = SUN8I_H3_THS_CLK_DIVIDER_WIDTH;
	div->table = sun8i_h3_ths_clk_table;
	div->lock = &sun8i_h3_ths_clk_lock;

	clk = clk_register_composite(NULL, clk_name, &parent, 1,
								 NULL, NULL,
								 &div->hw, &clk_divider_ops,
								 &gate->hw, &clk_gate_ops,
								 CLK_SET_RATE_PARENT);

	if (IS_ERR(clk))
		goto err_div_free;

	err = of_clk_add_provider(node, of_clk_src_simple_get, clk);
	if (err)
		goto err_unregister_clk;

	return;

err_unregister_clk:
	clk_unregister(clk);
err_gate_free:
	kfree(gate);
err_div_free:
	kfree(div);
err_unmap:
	iounmap(reg);
}

CLK_OF_DECLARE(sun8i_h3_ths_clk, "allwinner,sun8i-h3-ths-clk",
			   sun8i_h3_ths_clk_setup);
