#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/delay.h>

// Default setup as in E8C-SWTX board
static int gpio_clk = 0;
static int gpio_sel = 0;
static int gpio_data = 0;

module_param(gpio_clk,  int, 0660);
module_param(gpio_sel,  int, 0660);
module_param(gpio_data, int, 0660);

static uint32_t command_reg;
static uint32_t state_reg;
static uint32_t command_bits_set;
static uint32_t command_bits_reset;

enum tinyspi_gpio_modes {GPIO_READ, GPIO_WRITE};

void tinyspi_wait(void)
{
    // if CPLD clock is > 3 MHz, it should be enough.
    udelay(1); // three periods of CPLD clock
}

static int tinyspi_gpio_capture(int gpio_num, const char *gpio_label)
{
    if (!gpio_is_valid(gpio_num))
    {
        printk(KERN_ERR "tinyspi: invalid GPIO%d\n", gpio_num);
        return -EFAULT;
    }
    int error = gpio_request(gpio_num, gpio_label);
    if (error) printk(KERN_ERR "tinyspi: can't allocate GPIO%d\n", gpio_num);
    return error;
}

static void tinyspi_gpio_release(int gpio_num)
{
    gpio_free(gpio_num);
}

static void tinyspi_gpio_write(int gpio_num, int gpio_value)
{
    gpio_set_value(gpio_num, gpio_value);
}

static void tinyspi_gpio_conf(int gpio_num, enum tinyspi_gpio_modes gpio_mode, int gpio_value)
{
    int error = 0;

    if (gpio_mode == GPIO_READ)
        error = gpio_direction_input(gpio_num);

    if (gpio_mode == GPIO_WRITE)
        error = gpio_direction_output(gpio_num, gpio_value);

    if (error)
    {
        printk(KERN_ERR "tinyspi: unable to config GPIO%d to mode %d\n", gpio_num, (int)gpio_mode);
    }
}

static int  tinyspi_gpio_read   (int gpio_num)
{
    return gpio_get_value(gpio_num);
}

static void tinyspi_access(uint32_t *r_command_reg, uint32_t *r_state_reg, uint32_t w_command_bits_set, uint32_t w_command_bits_reset)
{
    int bit_counter;

    static uint32_t i_command_reg = 0;
    static uint32_t i_state_reg   = 0;

    // printk(KERN_DEBUG "tinyspi: calling tinyspi_access(%p[%08x],%p[%08x],%x,%x)\n", r_command_reg, r_command_reg ? *r_command_reg : 0, r_state_reg, r_state_reg ? *r_state_reg : 0, w_command_bits_set, w_command_bits_reset);

    // IDLE
    tinyspi_gpio_conf (gpio_sel,  GPIO_WRITE, 0);
    tinyspi_wait();

    // START
    tinyspi_gpio_conf (gpio_data, GPIO_READ,  0);
    tinyspi_gpio_conf (gpio_clk,  GPIO_WRITE, 0);
    tinyspi_wait();
    tinyspi_gpio_write(gpio_sel,  1);
    tinyspi_wait();

    // SLAVE LATCH
    tinyspi_gpio_write(gpio_clk,  1);
    tinyspi_wait();

    // MASTER READY
    tinyspi_gpio_write(gpio_clk,  0);
    tinyspi_wait();

    for(bit_counter = 0; bit_counter < 64; ++bit_counter)
    {
        uint32_t *target = (bit_counter < 32) ? &i_command_reg : &i_state_reg;

        // SLAVE OUT
        tinyspi_gpio_write(gpio_clk,  1);
        tinyspi_wait();

        // MASTER IN
        tinyspi_gpio_write(gpio_clk,  0);
        tinyspi_wait();
        int bit = tinyspi_gpio_read(gpio_data);
        tinyspi_wait();
        *target >>= 1;
        if (bit) *target |= 0x80000000;
    }

    if(r_command_reg) *r_command_reg = i_command_reg;
    if(r_state_reg)   *r_state_reg   = i_state_reg;

    // printk(KERN_DEBUG "tinyspi: registers has been read, cmd=%08x, state=%08x\n", i_command_reg, i_state_reg);

    if (!w_command_bits_set && !w_command_bits_reset)
    {
        // MASTER ABORT
        tinyspi_gpio_write(gpio_sel, 0);
        tinyspi_wait();
        tinyspi_gpio_conf (gpio_sel,  GPIO_READ, 0);
        tinyspi_gpio_conf (gpio_clk,  GPIO_READ, 0);
        tinyspi_wait();
        return;
    }

    i_command_reg |=  w_command_bits_set;
    i_command_reg &= ~w_command_bits_reset;

    // SLAVE RELEASE
    tinyspi_gpio_write(gpio_clk,  1);
    tinyspi_wait();
    tinyspi_gpio_conf (gpio_data, GPIO_WRITE, 0);
    tinyspi_wait();

    for(bit_counter = 0; bit_counter < 32; ++bit_counter)
    {
        // MASTER OUT
        tinyspi_gpio_write(gpio_clk,  0);
        tinyspi_wait();
        tinyspi_gpio_write(gpio_data, (i_command_reg >> bit_counter) & 1);
        tinyspi_wait();

        // SLAVE IN
        tinyspi_gpio_write(gpio_clk,  1);
        tinyspi_wait();
    }

    // MASTER RELEASE
    tinyspi_gpio_write(gpio_clk,  0);
    tinyspi_wait();
    tinyspi_gpio_conf (gpio_data, GPIO_READ, 0);
    tinyspi_wait();

    // SLAVE FINISH
    tinyspi_gpio_write(gpio_clk,  1);
    tinyspi_wait();
    tinyspi_gpio_write(gpio_clk,  0);
    tinyspi_wait();

    // STOP
    tinyspi_gpio_write(gpio_sel,  0);
    tinyspi_wait();
    tinyspi_gpio_conf (gpio_sel,  GPIO_READ, 0);
    tinyspi_gpio_conf (gpio_clk,  GPIO_READ, 0);
    tinyspi_wait();
}

static ssize_t command_reg_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    tinyspi_access(&command_reg, NULL, 0, 0);
    ssize_t size = sprintf(buf, "%08x\n", command_reg);
    return size;
}

static ssize_t command_reg_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    sscanf(buf, "%x", &command_reg);
    tinyspi_access(NULL, NULL, command_reg, ~command_reg);
    return count;
}

static ssize_t state_reg_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    tinyspi_access(NULL, &state_reg, 0, 0);
    ssize_t size = sprintf(buf, "%08x\n", state_reg);
    return size;
}

static ssize_t command_bits_set_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    sscanf(buf, "%x", &command_bits_set);
    tinyspi_access(NULL, NULL, command_bits_set, 0);
    return count;
}

static ssize_t command_bits_reset_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    sscanf(buf, "%x", &command_bits_reset);
    tinyspi_access(NULL, NULL, 0, command_bits_reset);
    return count;
}

static struct kobj_attribute command_reg_attribute        = __ATTR(command_reg,        0660, command_reg_show, command_reg_store);
static struct kobj_attribute state_reg_attribute          = __ATTR(state_reg,          0440, state_reg_show,   NULL);
static struct kobj_attribute command_bits_set_attribute   = __ATTR(command_bits_set,   0220, NULL,             command_bits_set_store);
static struct kobj_attribute command_bits_reset_attribute = __ATTR(command_bits_reset, 0220, NULL,             command_bits_reset_store);

static struct kobject *tinyspi_kobject;

static void tinyspi_cleanup(void)
{
    tinyspi_gpio_release(gpio_sel);
    tinyspi_gpio_release(gpio_clk);
    tinyspi_gpio_release(gpio_data);
    if(tinyspi_kobject) kobject_put(tinyspi_kobject);
}

static int __init tinyspi_init (void)
{
    int error = 0;

    if (!gpio_clk && !gpio_sel && !gpio_data)
    {
        printk(KERN_INFO "tinyspi: trying to autodetect TinySPI GPIO.\n");

        int offset;
        for (offset = 1; offset < 4096; ++offset)
        {
            if (!gpio_request(offset, "test"))
            {
                gpio_free(offset);
                break;
            }
        }
        if (offset == 4096) { printk(KERN_ERR "tinyspi: failed to autodetect GPIO offset\n"); return -ENOENT; }

        printk(KERN_INFO "tinyspi: first GPIO detected at %d.\n", offset);
        gpio_clk  = offset + 104;
        gpio_sel  = offset + 106;
        gpio_data = offset + 105;
    }

    printk(KERN_INFO "tinyspi: startup (CLK=GPIO%d, DATA=GPIO%d, SEL=GPIO%d).\n", gpio_clk, gpio_data, gpio_sel);
    tinyspi_kobject = kobject_create_and_add("tinyspi", kernel_kobj);
    if(!tinyspi_kobject) return -ENOMEM;

    error = sysfs_create_file(tinyspi_kobject, &command_reg_attribute.attr);
    if (error) { printk(KERN_ERR "tinyspi: failed to create: /sys/kernel/tinyspi/command_reg\n"); goto finish; }

    error = sysfs_create_file(tinyspi_kobject, &state_reg_attribute.attr);
    if (error) { printk(KERN_ERR "tinyspi: failed to create: /sys/kernel/tinyspi/state_reg\n"); goto finish; }

    error = sysfs_create_file(tinyspi_kobject, &command_bits_set_attribute.attr);
    if (error) { printk(KERN_ERR "tinyspi: failed to create: /sys/kernel/tinyspi/command_bits_set\n"); goto finish; }

    error = sysfs_create_file(tinyspi_kobject, &command_bits_reset_attribute.attr);
    if (error) { printk(KERN_ERR "tinyspi: failed to create: /sys/kernel/tinyspi/command_bits_reset\n"); goto finish; }

    error = tinyspi_gpio_capture(gpio_sel, "tinyspi_sel");
    if (error) { printk(KERN_ERR "tinyspi: failed to capture: GPIO%d (SEL)\n",  gpio_sel); goto finish; }

    error = tinyspi_gpio_capture(gpio_clk, "tinyspi_clk");
    if (error) { printk(KERN_ERR "tinyspi: failed to capture: GPIO%d (CLK)\n",  gpio_clk); goto finish; }

    error = tinyspi_gpio_capture(gpio_data, "tinyspi_data");
    if (error) { printk(KERN_ERR "tinyspi: failed to capture: GPIO%d (DATA)\n", gpio_data); goto finish; }

finish:
    if (error) tinyspi_cleanup();
    return error;
}

static void __exit tinyspi_exit (void)
{
    tinyspi_cleanup();
    printk(KERN_INFO "tinyspi: shutdown.\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Igor A. Molchanov");
MODULE_DESCRIPTION("Enables support for TinySPI interface for boards based on Elbrus-8C");
MODULE_VERSION("0.1");

module_init(tinyspi_init);
module_exit(tinyspi_exit);