#include <stdlib.h>
#include <gpiod.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <dbus/dbus.h>
#include <dirent.h>
#include <time.h>

static void msleep(long value)
{
    struct timespec rem, req = { value / 1000L, (value % 1000L) * 1000000L };
    while(nanosleep(&req, &rem)) req = rem;
}

static int readfile(const char *name, char **p_buf, long *p_size, void (*fini)(void))
{
    if((fini == NULL) || (p_buf == NULL)) return 8;

    FILE *f = fopen(name, "r");
    if (f == NULL) return 2;

    long size = 4096;
    if(p_size != NULL)
    {
        if (fseek(f, 0L, SEEK_END)) { fclose(f); return 3; }
        if ((size = ftell(f)) <= 0) { fclose(f); return 4; }
        rewind(f);
    }

    if ((*p_buf = malloc(size)) == NULL) { fclose(f); return 5; }
    atexit(fini);

    if (p_size != NULL) *p_size = size;
    size = fread(*p_buf, size, 1, f);
    if((p_size != NULL) && (size != 1)) { fclose(f); return 6; }
    if(fclose(f)) return 7;

    return 0;
}

static int writefile(const char *name, void *buf, long size)
{
    if(buf == NULL) return 9;

    FILE *f = fopen(name, "w");
    if (f == NULL) return 10;

    if(fwrite(buf, size, 1, f) != 1) { fclose(f); return 12; }
    if(fclose(f)) return 11;

    return 0;
}

char *s_config = NULL;
long s_config_len = 0;

static void config_fini(void)
{
    if (s_config) free(s_config);
    s_config = NULL;
}

static int config_init(void)
{
    if (s_config) return 0;
    return readfile("/etc/gpiotab", &s_config, &s_config_len, config_fini);
}

static char *find_config(const char *needle)
{
    int ndlen = strlen(needle);
    for(char *pos = s_config; (pos - s_config) <= s_config_len - (ndlen + 3); ++pos)
    {
        if (!strncmp(pos, needle, ndlen) && *(pos + ndlen) == '=')
        {
            char *rv = pos + ndlen + 1;
            if ((rv[0] < 'A') || (rv[0] > 'Z')) return NULL;
            if ((rv[1] >= '0') && (rv[1] <= '9')) return rv;
            if (rv + 2 >= s_config + s_config_len) return NULL;
            if ((rv[1] < 'A') || (rv[1] > 'Z')) return NULL;
            if ((rv[2] < '0') || (rv[2] > '9')) return NULL;
            return rv;
        }
    }
    return NULL;
}

static int gpio_to_num(const char *needle)
{
    int rv = config_init();
    if (rv) return -1;
    char *pos = find_config(needle);
    if (pos == NULL) return -2;
    if(pos[1] > '9')
    {
        return (pos[0] - 'A') * 208 + (pos[1] - 'A') * 8 + (pos[2] - '0');
    }
    else
    {
        return (pos[0] - 'A') * 8 + (pos[1] - '0');
    }
}

struct gpiod_chip *s_gpiochip = NULL;

static void gpiochip_fini(void)
{
    if (s_gpiochip) gpiod_chip_close(s_gpiochip);
    s_gpiochip = NULL;
}

static int gpiochip_init(void)
{
    if (s_gpiochip) return 0;
    s_gpiochip = gpiod_chip_open("/dev/gpiochip0");
    atexit(gpiochip_fini);
    if (s_gpiochip) return 0;
    return 13;
}

static int get_gpio(int num)
{
    int rv = gpiochip_init();
    if (rv) return rv;

    struct gpiod_line *line = gpiod_chip_get_line(s_gpiochip, num);
    if (line == NULL) return 14;

    rv = gpiod_line_request_input(line, "buttonscripts");
    if (rv) return 15;

    rv = gpiod_line_get_value(line);
    gpiod_line_release(line);
    return (rv < 0) ? 16 : rv;
}

static int set_gpio(int num, int value, int delay)
{
    int rv = gpiochip_init();
    if (rv) return rv;

    struct gpiod_line *line = gpiod_chip_get_line(s_gpiochip, num);
    if (line == NULL) return 17;

    rv = gpiod_line_request_output(line, "buttonscripts", value);
    if (rv) { gpiod_line_release(line); return 18; }

    msleep(delay);
    gpiod_line_release(line);

    rv = gpiod_line_request_input(line, "buttonscripts");
    gpiod_line_release(line);
    return rv ? 19 : 0;
}

static int get_power_state(void)
{
    int rv = gpio_to_num("GPIO_POWER_IN");
    return (rv >= 0) ? get_gpio(rv) : rv;
}

static int press_button(const char *sym, int delay)
{
    int rv = gpio_to_num(sym);
    return (rv >= 0) ? set_gpio(rv, 0, delay) : rv;
}

static int server_pwrbut_s(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
    return press_button("GPIO_PWR_BTN", 200);
}

static int server_pwrbut_h(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
    return press_button("GPIO_PWR_BTN", 5000);
}

static int server_reset(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
    return press_button("GPIO_RST_BTN", 200);
}

static int server_pwr_on(int argc, char *argv[])
{
    int power_state = get_power_state();
    switch(power_state)
    {
        case 1:  return 0;
        case 0:  return server_pwrbut_s(argc, argv);
        default: return power_state;
    }
}

static int server_pwr_off(int argc, char *argv[])
{
    int power_state = get_power_state();
    switch(power_state)
    {
        case 0:  return 0;
        case 1:  return server_pwrbut_s(argc, argv);
        default: return power_state;
    }
}

static int server_pwr_off_hard(int argc, char *argv[])
{
    int power_state = get_power_state();
    switch(power_state)
    {
        case 0:  return 0;
        case 1:  return server_pwrbut_h(argc, argv);
        default: return power_state;
    }
}

DBusConnection *s_dbus_conn = NULL;
DBusMessage *s_dbus_msg = NULL;
DBusMessage *s_dbus_reply = NULL;

static void dbus_fini(void)
{
    if (s_dbus_conn) dbus_connection_unref(s_dbus_conn);
    s_dbus_conn = NULL;
}

static void dbus_msg_fini(void)
{
    if (s_dbus_msg) dbus_message_unref(s_dbus_msg);
    s_dbus_msg = NULL;
}

static void dbus_reply_fini(void)
{
    if (s_dbus_reply) dbus_message_unref(s_dbus_reply);
    s_dbus_reply = NULL;
}

static int dbus_set_property(const char *service, const char *object, const char *interface, const char *property, int value)
{
    DBusError dbus_error;

    dbus_error_init(&dbus_error);
    if ((s_dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error)) == NULL) return 22;
    atexit(dbus_fini);

    if((s_dbus_msg = dbus_message_new_method_call(service, object, "org.freedesktop.DBus.Properties", "Set")) == NULL) return 20;
    atexit(dbus_msg_fini);

    if(!dbus_message_append_args(s_dbus_msg, DBUS_TYPE_STRING, &interface,  DBUS_TYPE_STRING, &property, DBUS_TYPE_INVALID)) return 21;

    DBusMessageIter dbus_iter, dbus_subiter;
    dbus_message_iter_init_append (s_dbus_msg, &dbus_iter);
    if (!dbus_message_iter_open_container (&dbus_iter, DBUS_TYPE_VARIANT, "b", &dbus_subiter)) return 23;
    if (!dbus_message_iter_append_basic(&dbus_subiter, DBUS_TYPE_BOOLEAN, &value)) { dbus_message_iter_abandon_container(&dbus_iter, &dbus_subiter); return 24; }
    if (!dbus_message_iter_close_container(&dbus_iter, &dbus_subiter)) return 25;

    if(!dbus_connection_send(s_dbus_conn, s_dbus_msg, NULL)) return 26;
    return 0;
}

static int dbus_get_property(const char *service, const char *object, const char *interface, const char *property)
{
    DBusError dbus_error;

    dbus_error_init(&dbus_error);
    if ((s_dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error)) == NULL) return 27;
    atexit(dbus_fini);

    if((s_dbus_msg = dbus_message_new_method_call(service, object, "org.freedesktop.DBus.Properties", "Get")) == NULL) return 28;
    atexit(dbus_msg_fini);

    if(!dbus_message_append_args(s_dbus_msg, DBUS_TYPE_STRING, &interface,  DBUS_TYPE_STRING, &property, DBUS_TYPE_INVALID)) return 29;

    if((s_dbus_reply = dbus_connection_send_with_reply_and_block(s_dbus_conn, s_dbus_msg, 1000, &dbus_error)) == NULL) return 30;
    atexit(dbus_reply_fini);

    DBusBasicValue value;
    DBusMessageIter dbus_iter, dbus_subiter;
    if (!dbus_message_iter_init(s_dbus_reply, &dbus_iter)) return 31;
    if (dbus_message_iter_get_arg_type (&dbus_iter) != DBUS_TYPE_VARIANT) return 32;
    dbus_message_iter_recurse (&dbus_iter, &dbus_subiter);
    if (dbus_message_iter_get_arg_type (&dbus_subiter) != DBUS_TYPE_BOOLEAN) return 33;
    dbus_message_iter_get_basic(&dbus_subiter, &value);
    return (value.bool_val) ? 1: 0;
}

static int dbus_watchdog_off(void)
{
    return dbus_set_property("xyz.openbmc_project.Watchdog", "/xyz/openbmc_project/watchdog/host0", "xyz.openbmc_project.State.Watchdog", "Enabled", 0);
}

static int server_watchdog_reset(int argc, char *argv[])
{
    /* 10 seconds should be fair enough for power to became on. */
    for(int sec = 10; sec > 0; --sec)
    {
        int power_state = get_power_state();
        switch(power_state)
        {
            case 1: return dbus_watchdog_off();
            case 0: break;
            default: return power_state;
        }
        msleep(1000);
    }
    return 35;
}

enum { UID_API_NONE, UID_API_DBUS, UID_API_TSPI, UID_API_GPIO } s_uid_api = UID_API_NONE;

DBusConnection *s_dbus_test_conn = NULL;
DBusMessage *s_dbus_test_msg = NULL;

static void dbus_test_fini(void)
{
    if (s_dbus_test_conn) dbus_connection_unref(s_dbus_test_conn);
    s_dbus_test_conn = NULL;
}

static void dbus_test_msg_fini(void)
{
    if (s_dbus_test_msg) dbus_message_unref(s_dbus_test_msg);
    s_dbus_test_msg = NULL;
}

char *s_reimucfg = NULL;
long s_reimucfg_len = 0;

static void reimucfg_fini(void)
{
    if (s_reimucfg) free(s_reimucfg);
    s_reimucfg = NULL;
}

static int chkdir(const char *path)
{
    DIR *d;
    if((d = opendir(path)) == NULL) return 1;
    if(closedir(d) != 0) return 2;
    return 0;
}

static int uid_init(void)
{
    if (s_uid_api != UID_API_NONE) return 0;

    /* Try to introspect LED manager. If success, consider DBUS protocol applicable. */

    int rv = 0;
    DBusError dbus_error;

    dbus_error_init(&dbus_error);
    if ((s_dbus_test_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error)) == NULL) return 36;
    atexit(dbus_test_fini);

    if((s_dbus_test_msg = dbus_message_new_method_call("xyz.openbmc_project.LED.GroupManager", "/", "org.freedesktop.DBus.Introspectable", "Introspect")) == NULL) return 37;
    atexit(dbus_test_msg_fini);

    DBusMessage *dbus_test_reply;
    if((dbus_test_reply = dbus_connection_send_with_reply_and_block(s_dbus_test_conn, s_dbus_test_msg, 1000, &dbus_error)) != NULL)
    {
        dbus_message_unref(dbus_test_reply);
        s_uid_api = UID_API_DBUS;
        return 0;
    }

    /* Try to check whether TinySPI is enabled, and tinyspi sysfs directory exist. If success, consider TSPI protocol applicable. */

    if((rv = readfile("/etc/reimu.conf", &s_reimucfg, &s_reimucfg_len, reimucfg_fini)) != 0) return 38;

    for(char *pos = s_reimucfg; (pos - s_reimucfg) <= s_reimucfg_len - 11; ++pos)
    {
        if (!strncmp(pos, "TINYSPI=yes", 11))
        {
            if(chkdir("/sys/kernel/tinyspi/")) return 39;
            s_uid_api = UID_API_TSPI;
            return 0;
        }
    }

    /* Try to check is LED sysfs directory exist. If success, consider GPIO protocol applicable. */

    if(chkdir("/sys/class/leds/platform:blue:uid/")) return 40;
    s_uid_api = UID_API_GPIO;
    return 0;
}

static int uid_set_dbus(int state)
{
    return dbus_set_property("xyz.openbmc_project.LED.GroupManager", "/xyz/openbmc_project/led/groups/enclosure_identify", "xyz.openbmc_project.Led.Group", "Asserted", state);
}

static int uid_get_dbus(void)
{
    return dbus_get_property("xyz.openbmc_project.LED.GroupManager", "/xyz/openbmc_project/led/groups/enclosure_identify", "xyz.openbmc_project.Led.Group", "Asserted");
}

char *s_led_state = NULL;

static void led_state_fini(void)
{
    if (s_led_state != NULL) free(s_led_state);
    s_led_state = NULL;
}

static int uid_set_gpio(int state)
{
    char *trig = state ? "default-on\n" : "none\n";
    return writefile("/sys/class/leds/platform:blue:uid/trigger", trig, strlen(trig));
}

static int uid_get_gpio(void)
{
    int rv = readfile("/sys/class/leds/platform:blue:uid/brightness", &s_led_state, NULL, led_state_fini);
    if (rv) return 41;
    if (!strncmp(s_led_state, "0\n", 2)) return 0;
    if (!strncmp(s_led_state, "255\n", 4)) return 1;
    return 42;
}

static int uid_set_tspi(int state)
{
    char *bits = state ? "00000004\n" : "00000008\n";
    if(writefile("/sys/kernel/tinyspi/command_bits_set", bits, strlen(bits))) return 43;
    msleep(10);
    if(writefile("/sys/kernel/tinyspi/command_bits_reset", bits, strlen(bits))) return 45;
    return 0;
}

static int uid_get_tspi(void)
{
    int rv = readfile("/sys/kernel/tinyspi/state_reg", &s_led_state, NULL, led_state_fini);
    if (rv) return 47;

    char *endptr;
    unsigned long res = strtoul(s_led_state, &endptr, 16);
    if (*endptr != '\n') return 46;
    return (res & 0x00000008) ? 1 : 0;
}

static int uid_set(int state)
{
    int rv = uid_init();
    if (rv) return rv;

    switch(s_uid_api)
    {
        case UID_API_DBUS: return uid_set_dbus(state);
        case UID_API_TSPI: return uid_set_tspi(state);
        case UID_API_GPIO: return uid_set_gpio(state);
        default: return 48;
    }
}

static int uid_get(void)
{
    int rv = uid_init();
    if (rv) return rv;

    switch(s_uid_api)
    {
        case UID_API_DBUS: return uid_get_dbus();
        case UID_API_TSPI: return uid_get_tspi();
        case UID_API_GPIO: return uid_get_gpio();
        default: return 49;
    }
}

static int uid_usage(int argc __attribute__((unused)), char *argv[])
{
    printf
    (
        "    Unit ID management utility for REIMU.\n"
        "    Usage:\n"
        "        %s off    - to set UID state off.\n"
        "        %s on     - to set UID state on.\n"
        "        %s switch - to invert current UID state.\n"
        "        %s query  - to query UID state (return code: 0 = off, 1 = on).\n"
        "        %s show   - to do the same as 'query', but also show UID state in terminal.\n",
        argv[0], argv[0], argv[0], argv[0], argv[0]
    );
    return 0;
}

static int uid_on(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
    return uid_set(1);
}

static int uid_off(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
    return uid_set(0);
}

static int uid_query(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
    return uid_get();
}

static int uid_show(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
    int uid_state = uid_get();
    switch(uid_state)
    {
        case 0:  printf("UID is OFF\n"); break;
        case 1:  printf("UID is ON\n"); break;
        default: fprintf(stderr, "Can't identify UID state!\n"); break;
    }
    return uid_state;
}

static int uid_switch(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
    int uid_state = uid_get();
    switch(uid_state)
    {
        case 0:  return uid_set(1);
        case 1:  return uid_set(0);
        default: fprintf(stderr, "Can't identify UID state!\n"); return uid_state;
    }
}

static int server_uid(int argc, char *argv[]);
struct
{
    const char *basename;
    int (*func)(int argc, char *argv[]);
} main_cmds[] =
{
    { "server_pwr_on", server_pwr_on },
    { "server_pwr_off", server_pwr_off },
    { "server_pwr_off_hard", server_pwr_off_hard },
    { "server_pwrbut_s", server_pwrbut_s },
    { "server_pwrbut_h", server_pwrbut_h },
    { "server_reset", server_reset },
    { "server_watchdog_reset", server_watchdog_reset },
    { "server_uid", server_uid },
    { NULL, NULL }
}, uid_cmds[] =
{
    { "on", uid_on },
    { "off", uid_off },
    { "query", uid_query },
    { "show", uid_show },
    { "switch", uid_switch },
    { "-h", uid_usage },
    { "--help", uid_usage }
};

static int server_uid(int argc, char *argv[])
{
    if (argc > 1)
    {
        for (int i = 0; uid_cmds[i].basename != NULL; ++i)
        {
            if (strcmp(argv[1], uid_cmds[i].basename)) continue;
            return uid_cmds[i].func(argc, argv);
        }
        fprintf(stderr, "You should specify a correct command.\n");
        uid_usage(argc, argv);
        return 50;
    }
    return uid_usage(argc, argv);
}

int main(int argc, char *argv[])
{
    char *cmd = basename(argv[0]);
    for (int i = 0; main_cmds[i].basename != NULL; ++i)
    {
        if (strcmp(cmd, main_cmds[i].basename)) continue;
        return main_cmds[i].func(argc, argv);
    }
    return 51;
}
