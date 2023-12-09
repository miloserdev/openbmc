#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dbus/dbus.h>
#include <gpiod.h>

char s_timestr[1024];

static char* gettime(void)
{
    time_t tm = time(NULL);
    strncpy(s_timestr, ctime(&tm), 1023);

    char *pos;
    for(pos = s_timestr; (*pos != '\0') && (*pos != '\n'); ++pos);
    *pos = '\0';

    return s_timestr;
}

enum msgtype {L_INFO, L_WARN};

static __attribute__((format(printf, 2, 3))) int message(enum msgtype type, const char *format, ...)
{
    char str[1024];
    va_list args;
    va_start (args, format);
    vsnprintf (str, 1023, format, args);
    va_end (args);

    FILE *stream = ((type == L_WARN) ? stderr : stdout);
    fprintf(stream, "reimupwr (%s): %s", gettime(), str);
    fflush(stream);
    return 0;
}

static void msleep(long value)
{
    struct timespec rem, req = { value / 1000L, (value % 1000L) * 1000000L };
    while(nanosleep(&req, &rem)) req = rem;
}

static int readfile(const char *name, char **p_buf, long *p_size)
{
    if(p_buf == NULL) return 8;

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

    if (p_size != NULL) *p_size = size;
    size = fread(*p_buf, size, 1, f);
    if((p_size != NULL) && (size != 1)) { free(*p_buf); fclose(f); return 6; }
    if(fclose(f)) { free(*p_buf); return 7; }

    return 0;
}

static int writefile(const char *name, const void *buf, long size)
{
    if(buf == NULL) return 8;

    FILE *f = fopen(name, "w");
    if (f == NULL) return 2;

    if(fwrite(buf, size, 1, f) != 1) { fclose(f); return 6; }
    if(fclose(f)) return 7;

    return 0;
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
    return 1;
}

static int get_gpio(int num)
{
    int rv = gpiochip_init();
    if (rv) return -1;

    struct gpiod_line *line = gpiod_chip_get_line(s_gpiochip, num);
    if (line == NULL) return -2;

    rv = gpiod_line_request_input(line, "overheatd");
    if (rv) return -3;

    rv = gpiod_line_get_value(line);
    gpiod_line_release(line);
    return (rv < 0) ? -4 : rv;
}

char *s_gpioconfig = NULL;
long s_gpioconfig_len = 0;

static void gpioconfig_fini(void)
{
    if (s_gpioconfig) free(s_gpioconfig);
    s_gpioconfig = NULL;
}

static int gpioconfig_init(void)
{
    if (s_gpioconfig) return 0;
    int rv = readfile("/etc/gpiotab", &s_gpioconfig, &s_gpioconfig_len);
    if (!rv) atexit(gpioconfig_fini);
    return rv;
}

static char *find_gpioconfig(const char *needle)
{
    int ndlen = strlen(needle);
    for(char *pos = s_gpioconfig; (pos - s_gpioconfig) <= s_gpioconfig_len - (ndlen + 3); ++pos)
    {
        if (!strncmp(pos, needle, ndlen) && *(pos + ndlen) == '=')
        {
            char *rv = pos + ndlen + 1;
            if ((rv[0] < 'A') || (rv[0] > 'Z')) return NULL;
            if ((rv[1] >= '0') && (rv[1] <= '9')) return rv;
            if (rv + 2 >= s_gpioconfig + s_gpioconfig_len) return NULL;
            if ((rv[1] < 'A') || (rv[1] > 'Z')) return NULL;
            if ((rv[2] < '0') || (rv[2] > '9')) return NULL;
            return rv;
        }
    }
    return NULL;
}

static int gpio_to_num(const char *needle)
{
    int rv = gpioconfig_init();
    if (rv) return -1;
    char *pos = find_gpioconfig(needle);
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

static int get_gpio_by_name(const char *name)
{
    int rv = gpio_to_num("GPIO_POWER_IN");
    return (rv >= 0) ? get_gpio(rv) : rv;
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

static int dbus_set_property_str(const char *service, const char *object, const char *interface, const char *property, const char *value)
{
    DBusError dbus_error;

    dbus_error_init(&dbus_error);
    if ((s_dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error)) == NULL) return 22;
    atexit(dbus_fini);

    dbus_msg_fini();
    if((s_dbus_msg = dbus_message_new_method_call(service, object, "org.freedesktop.DBus.Properties", "Set")) == NULL) return 20;
    atexit(dbus_msg_fini);

    if(!dbus_message_append_args(s_dbus_msg, DBUS_TYPE_STRING, &interface,  DBUS_TYPE_STRING, &property, DBUS_TYPE_INVALID)) return 21;

    DBusMessageIter dbus_iter, dbus_subiter;
    dbus_message_iter_init_append (s_dbus_msg, &dbus_iter);
    if (!dbus_message_iter_open_container (&dbus_iter, DBUS_TYPE_VARIANT, "s", &dbus_subiter)) return 23;
    if (!dbus_message_iter_append_basic(&dbus_subiter, DBUS_TYPE_STRING, &value)) { dbus_message_iter_abandon_container(&dbus_iter, &dbus_subiter); return 24; }
    if (!dbus_message_iter_close_container(&dbus_iter, &dbus_subiter)) return 25;

    if(!dbus_connection_send(s_dbus_conn, s_dbus_msg, NULL)) return 26;

    dbus_msg_fini();
    return 0;
}

int s_update_delay = 10;

static int s_update_delay_init(void)
{
    char *apo_cfg;
    int apo_cfg_len;
    int rv = readfile("/etc/auto_power_on", &apo_cfg, &apo_cfg_len);
    if (!rv)
    {
        char *psd_time = strstr(apo_cfg, "PS_DISCHARGE_TIME=");
        if(psd_time != NULL)
        {
            psd_time += strlen("PS_DISCHARGE_TIME=");
            char *endptr;
            long update_delay = strtol(psd_time, &endptr, 0);
            if (endptr != psd_time)
            {
                s_update_delay = update_delay;
                message(L_INFO, "Power supply discharge delay: %d seconds\n", s_update_delay);
            }
            else message(L_WARN, "Wrong data in auto_power_on config, defaulting power supply discharge delay to %d seconds\n", s_update_delay);
        }
        else message(L_INFO, "No power supply discharge delay specified in config, defaulting to %d seconds\n", s_update_delay);
        free(apo_cfg);
    }
    else message(L_WARN, "Can't read auto_power_on config, defaulting power supply discharge delay to %d seconds\n", s_update_delay);
    return s_update_delay;
}

int success = 0;

static void dbus_set_power_state(int pgood)
{
    const char *state = pgood ? "xyz.openbmc_project.State.Host.HostState.Running" : "xyz.openbmc_project.State.Host.HostState.Off";
    int rv;
    if ((rv = dbus_set_property_str("xyz.openbmc_project.State.Host", "/xyz/openbmc_project/state/host0", "xyz.openbmc_project.State.Host", "CurrentHostState", state)) != 0)
    {
        message(L_WARN, "Can't set host state to %s (error %d), deferring...\n", state, rv);
        success = 0;
    }
    else
    {
        success = 1;
    }
}

int poll_pgood()
{
    int powered_on;
    for(int i = 4; i >= 0; --i)
    {
        if(((powered_on = get_gpio_by_name("GPIO_POWER_IN")) < 0) && (i > 0))
        {
            message(L_WARN, "Warning: GPIO_POWER_IN is unavailable, retrying in 100 ms, %d attempts remaining\n", i);
            msleep(100);
            continue;
        }
        break;
    }
    if (powered_on < 0) { message(L_WARN, "Can't acquire GPIO_POWER_IN -- can't continue\n"); exit(2); }
    return powered_on;
}

int main(void)
{
    int old_pgood = -1;
    int update_delay = -1;

    message(L_INFO, "REIMU power control started\n");
    message(L_INFO, "Power supply discharge time is %d seconds\n", s_update_delay_init());
    for(;;)
    {
        int pgood = poll_pgood();
        if ((pgood != old_pgood) || !success)
        {
            message(L_INFO, "Power good state changed from %d to %d, changing power state\n", old_pgood, pgood);
            dbus_set_power_state(pgood);
            message(L_INFO, "Deferring power state being saved for %d seconds\n", s_update_delay);
            update_delay = s_update_delay;
            old_pgood = pgood;
        }
        if (update_delay == 0)
        {
            message(L_INFO, "Power good state %d is saved as last power state\n", pgood);
            char powerstate[32];
            snprintf(powerstate, 31, "%d\n", pgood);
            if (writefile("/var/lib/reimu/last_power_state", powerstate, strlen(powerstate)) != 0)
            {
                message(L_WARN, "Warning: can't save current power state (check if /var/lib/reimu/ exist and writable)\n");
            }
        }
        if (update_delay >= 0) --update_delay;
        msleep(1000);
    }
}
