#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include <gpiod.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <i2c/smbus.h>

int overheatd_enabled;
int debug;
long cpu_temp_limit;
long wait_msec;
long blink_msec;
long cpu_i2c_num;

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

static __attribute__((format(printf, 2, 3))) int error(int code, const char *format, ...)
{
    char str[1024];
    va_list args;
    va_start (args, format);
    vsnprintf (str, 1023, format, args);
    va_end (args);
    fprintf(stderr, "overheatd (%s): ERROR: %s", gettime(), str);
    exit(code);
}

static __attribute__((format(printf, 1, 2))) int dbg_printf(const char *format, ...)
{
    int rv = 0;
    char str[1024];

    if(debug)
    {
        va_list args;
        va_start (args, format);
        vsnprintf (str, 1023, format, args);
        va_end (args);
        rv = printf("overheatd (%s): %s", gettime(), str);
    }
    return rv;
}

static __attribute__((format(printf, 1, 2))) int wprintf(const char *format, ...)
{
    char str[1024];
    va_list args;
    va_start (args, format);
    vsnprintf (str, 1023, format, args);
    va_end (args);
    return fprintf(stderr, "overheatd (%s): %s", gettime(), str);
}

volatile int s_exit_req = 0;
volatile int s_exit_sig;

static void msleep(long value)
{
    struct timespec rem, req = { value / 1000L, (value % 1000L) * 1000000L };
    while(nanosleep(&req, &rem))
    {
        if(s_exit_req)
        {
            dbg_printf("Exiting on signal %d.\n", s_exit_sig);
            exit(0);
        }
        req = rem;
    }
}

static void exit_handler(int sig)
{
    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    s_exit_req = 1;
    s_exit_sig = sig;
}

static void rm_pidfile(void)
{
    unlink("/run/overheatd.pid");
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

static int chkdir(const char *path)
{
    DIR *d;
    if((d = opendir(path)) == NULL) return 1;
    if(closedir(d) != 0) return 2;
    return 0;
}

static int get_conf_bool(const char *confbuf, int conflen, const char *needle)
{
    int ndlen = strlen(needle);
    for(const char *pos = confbuf; (pos - confbuf) <= conflen - ndlen - 2; ++pos)
    {
        if (!strncmp(pos, needle, ndlen))
        {
            if (pos[ndlen] != '=') continue;

            pos += ndlen + 1;
            int toeol = conflen - (pos - confbuf);
            if ((toeol >= 4) && !strncmp(pos, "yes\n", 4)) return 1;
            if ((toeol >= 3) && !strncmp(pos, "no\n",  3)) return 0;
            if ((toeol >= 3) && !strncmp(pos, "on\n",  3)) return 1;
            if ((toeol >= 4) && !strncmp(pos, "off\n", 4)) return 0;
            return -2; /* Malformed data */
        }
    }
    return -1; /* Not found */
}

static long get_conf_long(const char *confbuf, int conflen, const char *needle)
{
    int ndlen = strlen(needle);
    for(const char *pos = confbuf; (pos - confbuf) <= conflen - ndlen - 2; ++pos)
    {
        if (!strncmp(pos, needle, ndlen))
        {
            if (pos[ndlen] != '=') continue;

            pos += ndlen + 1;

            char *endptr;
            long rv = strtol(pos, &endptr, 0);
            if (*endptr != '\n') return -2; /* Malformed data */
            return rv;
        }
    }
    return -1; /* Not found */
}

int s_tinyspi = -1;

static int detect_tinyspi(void)
{
    if (s_tinyspi < 0)
    {
        char *reimucfg;
        long reimucfg_len;
        if(readfile("/etc/reimu.conf", &reimucfg, &reimucfg_len) != 0) error(91, "Unable to read /etc/reimu.conf\n");
        if ((s_tinyspi = get_conf_bool(reimucfg, reimucfg_len, "TINYSPI")) < 0) { free(reimucfg); error(92, "No TINYSPI entry in /etc/reimu.conf\n"); }
        free(reimucfg);
        if (s_tinyspi && chkdir("/sys/kernel/tinyspi/")) error(90, "TinySPI is enabled, but no API files exported by kernel\n");
    }

    return s_tinyspi;
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
    int rv = gpio_to_num(name);
    return (rv >= 0) ? get_gpio(rv) : rv;
}

static int detect_cpus(int cpu_i2c_num, int *cpus)
{
    char dev[50];
    snprintf(dev, 50, "/dev/i2c-%d", cpu_i2c_num);

    int file = open(dev, O_RDWR);
    if (file == -1) error(78, "I2C bus %d failure\n", cpu_i2c_num);

    for(int i = 0; i <=3; ++i)
    {
        int rv, slave = 0x20 + 0x10 * i;
        cpus[i] = 0;
        dbg_printf("I2C_SLAVE IOCTL for %s, slave 0x%02x returned %d\n", dev, slave, rv = ioctl(file, I2C_SLAVE, slave));
        if (rv < 0) continue;
        dbg_printf("SMBus Write Quick returned %d\n", rv = i2c_smbus_write_quick(file, I2C_SMBUS_WRITE));
        if (rv != 0) continue;
        cpus[i] = 1;
    }
    close(file);
    return 0;
}

static int get_i2c_temperature(int cpu_i2c_num, int cpu, int sensor, double *result)
{
    char dev[50];
    snprintf(dev, 50, "/dev/i2c-%d", cpu_i2c_num);

    int file = open(dev, O_RDWR);
    if (file == -1) error(79, "I2C bus %d failure\n", cpu_i2c_num);

    int rv = ioctl(file, I2C_SLAVE, 0x20 + cpu * 0x10 + sensor);
    if (rv) error(80, "I2C_SLAVE IOCTL for %s, cpu %d, sensor %d unexpectedly returned %d\n", dev, cpu, sensor, rv);

    uint8_t bytes[2];
    rv = (read(file, bytes, 2) != 2);
    close(file);
    if (rv) return -1;

    int temp = bytes[0];
    temp <<= 8;
    temp |= bytes[1];
    *result = temp;
    *result /= 8;
    temp /= 8;
    return temp;
}

static int overheat_set_led_gpio(const char *trigger)
{
    char *triggerlf;
    if ((triggerlf = malloc(strlen(trigger) + 2)) == NULL) error(94, "Out of memory\n");
    strcpy(triggerlf, trigger);
    strcat(triggerlf, "\n");

    const char *triggerfiles[] =
    {
        "/sys/class/leds/platform:red:overheat/trigger",
        "/sys/class/leds/platform:red:ohfanfail/trigger",
        "/sys/class/leds/platform:red:ohfanfail2/trigger",
        NULL
    };

    const char *delayfiles[] =
    {
        "/sys/class/leds/platform:red:overheat/delay_on",
        "/sys/class/leds/platform:red:ohfanfail/delay_on",
        "/sys/class/leds/platform:red:ohfanfail2/delay_on",
        "/sys/class/leds/platform:red:overheat/delay_off",
        "/sys/class/leds/platform:red:ohfanfail/delay_off",
        "/sys/class/leds/platform:red:ohfanfail2/delay_off",
        NULL
    };

    for(int trig = 0; triggerfiles[trig]; ++trig)
    {
        if(writefile(triggerfiles[trig], triggerlf, strlen(triggerlf))) error(16, "Can't set %s to %s\n", triggerfiles[trig], trigger);
    }

    if (!strcmp(trigger,"timer"))
    {
        char msec[16];
        sprintf(msec, "%ld\n", blink_msec);
        for(int trig = 0; delayfiles[trig]; ++trig)
        {
            if(writefile(delayfiles[trig], msec, strlen(msec))) error(16, "Can't set delay %ld for %s\n", blink_msec, triggerfiles[trig]);
        }
    }

    dbg_printf("Changed LED state trigger to %s through sysfs\n", trigger);
}

volatile int s_timer_enable = 0;
volatile int s_tspi_led = 0;
timer_t s_tspi_timer;
int s_tspi_timer_ready = 0;
int s_old_blink_msec = -1;
struct itimerspec s_blink_its = {{0, 0}, {0, 0}};

static void overheat_control_led_tspi(int status, int nodbg)
{
    const char *ledfile = status ? "/sys/kernel/tinyspi/command_bits_reset" : "/sys/kernel/tinyspi/command_bits_set";
    if(writefile(ledfile, "0x00000040\n", 11)) error(17, "Can't write to %s\n", ledfile);
    if (!nodbg) dbg_printf("Changed LED state through %s\n", ledfile);
}

static void tspi_led_handler(int sig __attribute__((unused)))
{
    if (s_timer_enable)
    {
        s_tspi_led = 1 - s_tspi_led;
        overheat_control_led_tspi(s_tspi_led, 1);
    }
    signal(SIGALRM, tspi_led_handler);
}

static void tspi_led_fini(void)
{
    if (s_tspi_timer_ready) timer_delete(s_tspi_timer);
}

static void tspi_led_init(void)
{
    if (!s_tspi_timer_ready)
    {
        timer_create(CLOCK_MONOTONIC, NULL, &s_tspi_timer);
        signal(SIGALRM, tspi_led_handler);
        atexit(tspi_led_fini);
        dbg_printf("Created TinySPI LED timer\n");
        s_tspi_timer_ready = 1;
    }
    if(s_timer_enable && (s_old_blink_msec != blink_msec))
    {
        if(timer_settime(s_tspi_timer, 0, &s_blink_its, NULL)) error(102, "Unable to set up TinySPI timer\n");
        s_old_blink_msec = blink_msec;
    }
}

static int overheat_set_led_tspi(const char *trigger)
{
    tspi_led_init();
    if(!strcmp(trigger, "timer"))
    {
        if (!s_tspi_timer_ready) error(102, "TinySPI timer failed\n");;
        dbg_printf("Enabled TinySPI LED timer\n");
        s_timer_enable = 1;
    }
    else
    {
        if (!s_tspi_timer_ready) error(102, "TinySPI timer failed\n");;
        dbg_printf("Disabled TinySPI LED timer\n");
        s_timer_enable = 0;
        s_tspi_led = (!strcmp(trigger, "default-on")) ? 1 : 0;
        overheat_control_led_tspi(s_tspi_led, 0);
    }
}

char *s_statefile = NULL;

static int statefile_commit(const char *filename)
{
    int rv = (filename == NULL) ? 0 : writefile(filename, s_statefile, strlen(s_statefile));
    if ((s_statefile = realloc(s_statefile, 2)) == NULL) error(94, "Out of memory\n");
    strcpy(s_statefile,"");
    return rv;
}

static int __attribute__((format(printf, 1, 2))) statefile_append(const char *format, ...)
{
    char str[1024];
    va_list args;
    va_start (args, format);
    vsnprintf (str, 1023, format, args);
    va_end (args);
    if ((s_statefile = realloc(s_statefile, strlen(s_statefile) + strlen(str) + 1)) == NULL) error(93, "Out of memory\n");
    strcat(s_statefile, str);
}

static uint32_t get_alerts_tinyspi(void)
{
    /* Reset TinySPI alert */
    if(writefile("/sys/kernel/tinyspi/command_bits_set", "00000010\n", 9)) error(3, "Can't start resetting TinySPI alert");
    msleep(10);
    if(writefile("/sys/kernel/tinyspi/command_bits_reset", "00000010\n", 9)) error(5, "Can't finish resetting TinySPI alert");

    /* Read status register */
    char *tinyspi_state;
    int rv = readfile("/sys/kernel/tinyspi/state_reg", &tinyspi_state, NULL);
    if (rv) error(64, "Can't read TinySPI status register");

    char *endptr;
    uint32_t res = strtoul(tinyspi_state, &endptr, 16);
    if (*endptr != '\n') error(65, "Wrong data in TinySPI status register");
    free(tinyspi_state);
    return res;
}

static uint32_t get_alerts_gpio(void)
{
    /* TinySPI has the following assignment, so we try to adopt it:
     * INTRUSION_SW# = bit 4  = INTRUSION_SW#
     * I2C0_ALERT#   = bit 6  = ALERT_MEM#
     * I2C1_ALERT#   = bit 8  = ALERT_SMBUS#
     * I2C1_TCRIT#   = bit 9  = TCRIT_SMBUS#
     * I2C1_FAULT#   = bit 10 = ALERT_FRU#
     * I2C2_ALERT#   = bit 11 = ALERT_PCIE#
     * I2C3_ALERT#   = bit 12 = ALERT_CPU2#
     * I2CM_ALERT#   = bit 14 = ALERT_CPU#
     * PWROK_MAIN    = bit 21 = PWROK_ATX
     * APMDZ_LED#    = bit 23 = APMDZ_LED#
     * GPI[7] (RFU)  = bit 31 = PLT_RST#
     */

    uint32_t b[32];
    memset(b, 0, sizeof(b));
    b[31] = get_gpio_by_name("GPIO_RESET_IN");
    b[23] = get_gpio_by_name("GPIO_APMDZ_LED");
    b[21] = get_gpio_by_name("GPIO_POWER_IN");
    b[14] = get_gpio_by_name("GPIO_ALERT_CPU");
    b[12] = get_gpio_by_name("GPIO_ALERT_CPU2");
    b[6]  = get_gpio_by_name("GPIO_ALERT_MEM");
    b[11] = get_gpio_by_name("GPIO_ALERT_PCIE");
    b[10] = get_gpio_by_name("GPIO_ALERT_FRU");
    b[8]  = get_gpio_by_name("GPIO_ALERT_SMBUS");
    b[9]  = get_gpio_by_name("GPIO_TCRIT_SMBUS");
    b[4]  = get_gpio_by_name("GPIO_INTRUSION");

    uint32_t alerts = 0;
    for (int i = 0; i < 32; ++i)
    {
        if (b[i] < 0) error(3, "Can't acquire alert GPIO #%d\n", i);
        alerts |= (b[i] << i);
    }

    return alerts | 0x0010a0a5;
}

volatile int s_dbg_state = 0; /* 0 = normal state; 1 = slight overheat; 2 = critical overheat */

static void set_dbg_state(int sig __attribute__((unused)))
{
    s_dbg_state = (s_dbg_state + 1) % 3;
    signal(SIGUSR1, set_dbg_state);
}

static int chkfile(const char *file)
{
  struct stat st;
  if(stat(file, &st)) return -1;
  if(!S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode)) return -2;
  return 0;
}

static void reload_config(const char *configfile)
{
    if (chkfile(configfile))
    {
        /* If no configfile exist, create it there */

        const char *defaultconfig =
            "OVERHEATD_ENABLED=yes\n"
            "DEBUG=off\n"
            "CPU_TEMP_LIMIT=85\n"
            "WAIT_MSEC=5000\n"
            "BLINK_MSEC=500\n"
            "CPU_I2C_NUM=3\n";

        if(writefile(configfile, defaultconfig, strlen(defaultconfig))) error(77, "Can't create new config %s\n", defaultconfig);
    }

    char *daemoncfg;
    long daemoncfg_len;

    if(readfile(configfile, &daemoncfg, &daemoncfg_len)) error(91, "Unable to read /etc/reimu.conf\n");

    overheatd_enabled = get_conf_bool(daemoncfg, daemoncfg_len, "OVERHEATD_ENABLED");
    debug             = get_conf_bool(daemoncfg, daemoncfg_len, "DEBUG");
    cpu_temp_limit    = get_conf_long(daemoncfg, daemoncfg_len, "CPU_TEMP_LIMIT");
    wait_msec         = get_conf_long(daemoncfg, daemoncfg_len, "WAIT_MSEC");
    blink_msec        = get_conf_long(daemoncfg, daemoncfg_len, "BLINK_MSEC");
    cpu_i2c_num       = get_conf_long(daemoncfg, daemoncfg_len, "CPU_I2C_NUM");

    free(daemoncfg);

    if((overheatd_enabled < 0) || (debug < 0) || (cpu_temp_limit < 0) || (wait_msec < 0) || (blink_msec < 0) || (cpu_i2c_num < 0))
    {
        error(96, "Malformed values in %s\n", configfile);
    }

    s_blink_its.it_value.tv_sec  = blink_msec / 1000L;
    s_blink_its.it_value.tv_nsec = (blink_msec % 1000L) * 10000000L;
    s_blink_its.it_interval = s_blink_its.it_value;
}

int main(int argc, char *argv[])
{
    const char *led_trigger = "";
    uint32_t alerts = 0;
    int tinyspi_alert = 2; /* Unconditionally acquire TinySPI data */
    int overheat_cond = 0;
    int last_dbg_state = 0;

    signal(SIGTERM, exit_handler);
    signal(SIGHUP, exit_handler);
    signal(SIGINT, exit_handler);
    signal(SIGUSR1, set_dbg_state);

    char pid[10];
    sprintf(pid, "%d\n", getpid());
    atexit(rm_pidfile);
    if(writefile("/run/overheatd.pid", pid, strlen(pid))) error(127, "Can't create pid file\n");
    statefile_commit(NULL);

    for(;;)
    {
        reload_config("/etc/overheatd.conf");
        dbg_printf("Reloaded config: (OVERHEATD_ENABLED = %d, DEBUG = %d; CPU_TEMP_LIMIT = %ld; WAIT_MSEC = %ld; BLINK_MSEC = %ld; CPU_I2C_NUM = %ld)\n", overheatd_enabled, debug, cpu_temp_limit, wait_msec, blink_msec, cpu_i2c_num);

        if(overheatd_enabled)
        {
            statefile_append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<state date=\"%s\">\n", gettime());

            if (last_dbg_state != s_dbg_state)
            {
                tinyspi_alert = 2;
                last_dbg_state = s_dbg_state;
            }

            int powered_on;
            const char *oldledtrigger = led_trigger;
            led_trigger = "none";

            /* Check current power state */
            for(int i = 4; i >= 0; --i)
            {
                if(((powered_on = get_gpio_by_name("GPIO_POWER_IN")) < 0) && (i > 0))
                {
                    wprintf("Warning: GPIO_POWER_IN is unavailable, retrying in 100 ms, %d attempts remaining\n", i);
                    msleep(100);
                    continue;
                }
                break;
            }
            if (powered_on < 0) error(2, "Can't acquire GPIO_POWER_IN\n");

            /* Re-read alerts, if needed */
            if (detect_tinyspi())
            {
                int check = (tinyspi_alert == 2);
                if ((tinyspi_alert = get_gpio_by_name("GPIO_TINYSPI_ALERT")) < 0) error(3, "Can't acquire TinySPI alert\n");

                if(check || !tinyspi_alert)
                {
                    dbg_printf("Re-reading alerts (tinyspi_alert = %d, old alerts = 0x%08x)\n", tinyspi_alert, alerts);
                    alerts = get_alerts_tinyspi();
                }
            }
            else
            {
                dbg_printf("Re-reading alerts (old alerts = 0x%08x)\n", alerts);
                alerts = get_alerts_gpio();
            }

            dbg_printf("Alerts: 0x%08x\n", alerts);
            statefile_append("\t<alerts>0x%08x</alerts>\n", alerts);

            if (s_dbg_state == 2) { alerts &= 0xffff7fff; dbg_printf("DEBUG MODE: alerts altered to 0x%08x\n", alerts); }

            /* THERMAL_SHDN# (bit 0) == 0 : overheat on */
            if ((alerts & 0x00000001) == 0)
            {
                led_trigger = "default-on";
                overheat_cond = 1;
            }

            /* I2CM_TTRIP# (bit 15) == 0 & PWROK_MAIN (bit 21) == 1 : overheat on */
            if ((alerts & 0x00200000) != 0)
            {
                if ((alerts & 0x00008000) == 0)
                {
                    led_trigger = "default-on";
                    overheat_cond = 1;
                }
                else
                {
                    overheat_cond = 0;
                }
            }
            dbg_printf("After checking TTRIP: led_trigger: %s, overheat_cond: %d\n", led_trigger, overheat_cond);

            /* I2CM_TTRIP# (bit 15) == 0 & PWROK_MAIN (bit 21) == 0, but overheat condition is reached : overheat on */
            if ((alerts & 0x00008000) == 0)
            {
                if ((alerts & 0x00200000) == 0)
                {
                    led_trigger = overheat_cond ? "default-on" : "none";
                }
            }
            dbg_printf("led_trigger after checking overheat_cond: %s\n", led_trigger);

            /* We are not in critical overheat, and powered on, so check I2C */
            if(!strcmp(led_trigger, "none"))
            {
                if (powered_on)
                {
                    /* Detect available cpus */
                    int cpus[4] = {0, 0, 0, 0};
                    detect_cpus(cpu_i2c_num, cpus);
                    dbg_printf("Bus %ld, CPUs detected: %d, %d, %d, %d\n", cpu_i2c_num, cpus[0], cpus[1], cpus[2], cpus[3]);
                    statefile_append("\t<cpus>");
                    int first = 1;
                    for (int cpu = 0; cpu <= 3; ++cpu)
                    {
                        if (cpus[cpu])
                        {
                            statefile_append(first ? "%d" : " %d", cpu);
                            first = 0;
                        }
                    }
                    statefile_append("<cpus>\n");

                    /* Check temperature of each cpu sensor */
                    int maxtemp = 0;
                    for (int cpu = 0; cpu <= 3; ++cpu)
                    {
                        if (cpus[cpu])
                        {
                            for (int sensor = 0; sensor <= 7; ++sensor)
                            {
                                int temp;
                                double tempdbl;
                                if((temp = get_i2c_temperature(cpu_i2c_num, cpu, sensor, &tempdbl)) < 0)
                                {
                                    wprintf("Failure reading I2C device (bus = %ld, cpu = %d, sensor = %d)", cpu_i2c_num, cpu, sensor);
                                }
                                dbg_printf("Temp (%d:%d): %d (%f)\n", cpu, sensor, temp, tempdbl);
                                statefile_append("\t<temperature cpu=\"%d\" sensor=\"%d\">%f</temperature>\n", cpu, sensor, tempdbl);
                                if (temp > maxtemp) maxtemp = temp;
                            }
                        }
                    }
                    dbg_printf("maxtemp: %d\n", maxtemp);
                    if (s_dbg_state == 1) { maxtemp = cpu_temp_limit + 1; dbg_printf("DEBUG MODE: maxtemp altered to %d\n", maxtemp); }

                    /* Any of temperatures is greater than limit */
                    if (maxtemp > cpu_temp_limit) led_trigger = "timer";
                }
            }

            /* Update LEDs, if needed */
            dbg_printf("LED trigger: %s -> %s\n", oldledtrigger, led_trigger);
            if (strcmp(oldledtrigger, led_trigger))
            {
                if(detect_tinyspi())
                {
                    overheat_set_led_tspi(led_trigger);
                }
                else
                {
                    overheat_set_led_gpio(led_trigger);
                }
            }

            /* Finalize statefile */
            statefile_append("</state>\n");
            if(statefile_commit("/var/volatile/systemstate.xml")) error(24, "Can't update statefile\n");
        }

        /* Sleep till next cycle */
        msleep(wait_msec);
    }
}
