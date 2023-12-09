# Get back rmd160 (needed by libssh2)
EXTRA_OECONF_remove_class-target = "no-rmd160"

# Get back md4 and rc4 (needed by php)
EXTRA_OECONF_remove_class-target = "no-md4"
EXTRA_OECONF_remove_class-target = "no-rc4"
