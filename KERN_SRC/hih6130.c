#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>

struct hih6130_dev {
	struct i2c_client *client;

	int humidity;
	int temperature;
	struct mutex lock;
};

static int hih6130_update_data(struct hih6130_dev *dev)
{
	int ret = 0;
	char data[4];
	char dummy = 0;
	struct i2c_client *client = dev->client;

	mutex_lock(&dev->lock);

	/* As per the data sheet, a single write command has to be issued
	 * without any data. As Linux doesn't have any APIs to issue only
	 * the write command, we are using the send API to issue the write
	 * command and further send a dummy one byte value, say 0.
	 */
	ret = i2c_master_send(client, &dummy, 1);
	if (ret < 0)
		goto out;

	/* As per the data sheet, it takes around 40ms to read, process and
	 * update the device registers
	 */
	msleep(40);

	ret = i2c_master_recv(client, data, 4);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to read the data\n");
		return 0;
	}

	/* Check the status bits and return if something goes wrong*/
	if (data[0] & 0xC0) {
		dev_err(&client->dev, "Failed to fetch the proper value\n");
		ret = -EINVAL;
		goto out;
	}

	dev->humidity = ((data[0] << 8) + data[1]) & 0x3fff;
	dev->temperature = ((data[2] << 8) + data[3]) >> 2;

	/* As of this point, the temperature, say 26.542 is stored as 26542
	 * Same is the case with humidity
	 */
	dev->humidity = DIV_ROUND_CLOSEST(dev->humidity * 1000, 16383) * 100;
	dev->temperature = DIV_ROUND_CLOSEST(
				dev->temperature * 1000 * 165, 16383) - 40000;

#ifdef HIH6130_DEBUG
	dev_info(&client->dev, "Humidity : %u\n", dev->humidity);
	dev_info(&client->dev, "Temperature : %u\n", dev->temperature);
#endif

out:
	mutex_unlock(&dev->lock);

	return ret;
}

static int hih6130_probe(struct i2c_client *client,
		const struct i2c_device_id *i2c_device_id)
{
	struct hih6130_dev *dev;

	dev = devm_kzalloc(&client->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->client = client;

	mutex_init(&dev->lock);

	i2c_set_clientdata(client, dev);

	return 0;
}

static const struct i2c_device_id hih6130_id[] = {
		{"hih6130", 0},
		{ }
};

static struct i2c_driver hih6130_driver = {
		.driver = {
				.name = "hih6130"
		},
		.id_table = hih6130_id,
		.probe = hih6130_probe,
};

module_i2c_driver(hih6130_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A Raghavendra Rao <arrao@cdac.in>");
