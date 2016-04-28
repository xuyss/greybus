/*
 * Greybus Firmware Core Bundle Driver.
 *
 * Copyright 2016 Google Inc.
 * Copyright 2016 Linaro Ltd.
 *
 * Released under the GPLv2 only.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/firmware.h>
#include "greybus.h"

struct gb_fw_core {
	struct gb_connection	*mgmt_connection;
};

static int gb_fw_core_probe(struct gb_bundle *bundle,
			    const struct greybus_bundle_id *id)
{
	struct greybus_descriptor_cport *cport_desc;
	struct gb_connection *connection;
	struct gb_fw_core *fw_core;
	int ret, i;
	u16 cport_id;
	u8 protocol_id;

	fw_core = kzalloc(sizeof(*fw_core), GFP_KERNEL);
	if (!fw_core)
		return -ENOMEM;

	/* Parse CPorts and create connections */
	for (i = 0; i < bundle->num_cports; i++) {
		cport_desc = &bundle->cport_desc[i];
		cport_id = le16_to_cpu(cport_desc->id);
		protocol_id = cport_desc->protocol_id;

		switch (protocol_id) {
		case GREYBUS_PROTOCOL_FW_MANAGEMENT:
			/* Disallow multiple Firmware Management CPorts */
			if (fw_core->mgmt_connection) {
				dev_err(&bundle->dev,
					"multiple management CPorts found\n");
				ret = -EINVAL;
				goto err_destroy_connections;
			}

			connection = gb_connection_create(bundle, cport_id,
							  NULL);
			if (IS_ERR(connection)) {
				ret = PTR_ERR(connection);
				dev_err(&bundle->dev,
					"failed to create management connection (%d)\n",
					ret);
				goto err_free_fw_core;
			}

			fw_core->mgmt_connection = connection;
			break;
		default:
			dev_err(&bundle->dev, "invalid protocol id (0x%02x)\n",
				protocol_id);
			ret = -EINVAL;
			goto err_free_fw_core;
		}
	}

	/* Firmware Management connection is mandatory */
	if (!fw_core->mgmt_connection) {
		dev_err(&bundle->dev, "missing management connection\n");
		ret = -ENODEV;
		goto err_free_fw_core;
	}

	greybus_set_drvdata(bundle, fw_core);

	return 0;

err_destroy_connections:
	gb_connection_destroy(fw_core->mgmt_connection);
err_free_fw_core:
	kfree(fw_core);

	return ret;
}

static void gb_fw_core_disconnect(struct gb_bundle *bundle)
{
	struct gb_fw_core *fw_core = greybus_get_drvdata(bundle);

	gb_connection_destroy(fw_core->mgmt_connection);

	kfree(fw_core);
}

static const struct greybus_bundle_id gb_fw_core_id_table[] = {
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_FW_MANAGEMENT) },
	{ }
};

static struct greybus_driver gb_fw_core_driver = {
	.name		= "gb-firmware",
	.probe		= gb_fw_core_probe,
	.disconnect	= gb_fw_core_disconnect,
	.id_table	= gb_fw_core_id_table,
};

static int fw_core_init(void)
{
	return greybus_register(&gb_fw_core_driver);
}
module_init(fw_core_init);

static void __exit fw_core_exit(void)
{
	greybus_deregister(&gb_fw_core_driver);
}
module_exit(fw_core_exit);

MODULE_ALIAS("greybus:firmware");
MODULE_AUTHOR("Viresh Kumar <viresh.kumar@linaro.org>");
MODULE_DESCRIPTION("Greybus Firmware Bundle Driver");
MODULE_LICENSE("GPL v2");