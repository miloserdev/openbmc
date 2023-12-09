#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <rfb/rfbproto.h>

char *fname = NULL;

static __attribute__((format(printf, 2, 3))) int error(int code, const char *format, ...)
{
    va_list args;
    va_start (args, format);
    vfprintf (stderr, format, args);
    va_end (args);
    if (fname) free(fname);
    exit(code);
}

int main(int argc, char *argv[])
{
    int pass_max = sysconf(_SC_PASS_MAX);
    if (pass_max < 0) pass_max = 8192;

    if (argc > 1)
    {
        fname = malloc(strlen(argv[1]) + 1);
        if (fname == NULL) error(4, "Out of memory for destination file name\n");
        strcpy(fname, argv[1]);
    }
    else
    {
        const char *homedir;
        if ((homedir = getenv("HOME")) == NULL)
        {
            homedir = getpwuid(getuid())->pw_dir;
            if (strlen(homedir) < 1) error(3, "Can't determine home directory\n");
        }

        const char *default_fname = "/.vnc/passwd";
        fname = malloc(strlen(homedir) + strlen(default_fname) + 1);
        if (fname == NULL) error(4, "Out of memory for destination file name\n");
        strcpy(fname, homedir);
        strcat(fname, default_fname);
    }

    char pass1[pass_max + 1], pass2[pass_max + 1],  *pass;
    pass = getpass("Password: ");
    if (!pass) error(1, "Can't read password\n");
    strncpy(pass1, pass, pass_max);

    pass = getpass("Verify: ");
    if (!pass) error(1, "Can't read password verification\n");
    strncpy(pass2, pass, pass_max);

    if (strcmp(pass1, pass2)) error(2, "Passwords don't match\n");

    int rv;
    if ((rv = rfbEncryptAndStorePasswd(pass1, fname)) != 0) error(rv, "Can't save password to %s (error %d)\n", fname, rv);
    free(fname);
    return 0;
}
