inherit reimu-version

def getstatusoutput(cmd):
    import subprocess
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    return p.wait(), p.communicate()

def get_openbmc_commit(d):
    import os
    version = 'unknown-rev'

    cur = os.path.realpath(os.getcwd())
    is_openbmc_root = lambda cur: \
        os.path.isdir(os.path.join(cur, '.git')) and \
        os.path.isfile(os.path.join(cur, 'oe-init-build-env'))

    while cur and cur != '/' and not is_openbmc_root(cur):
        cur = os.path.dirname(cur)

    bb.debug(2, 'Found OpenBMC root %s, is_openbmc=%s'
             % (cur, cur and is_openbmc_root(cur)))
    gitdir = os.path.join(cur, '.git')

    if cur and is_openbmc_root:
        cmd = ['git', '--git-dir=%s' % gitdir,
               '--work-tree=%s' % cur,
               'rev-parse', '--short', 'HEAD']
        exitstatus, output = getstatusoutput(cmd)
        if exitstatus == 0:
            version = output[0].decode("utf-8")[:7]
    return version

ISSUE_COMMIT := "${@get_openbmc_commit(d)}"

do_alter_dates() {
    sed -i "s/(Phosphor OpenBMC Project Reference Distro) 0.1.0/(MCST ${MACHINE_STRING} ${REIMU_VERSION} built at `date +'%Y-%m-%d %H:%M %Z'`, git @${ISSUE_COMMIT})/g" ${IMAGE_ROOTFS}/usr/lib/os-release ${IMAGE_ROOTFS}/etc/issue ${IMAGE_ROOTFS}/etc/issue.net
}

do_rootfs[vardepsexclude] += "ISSUE_COMMIT"

ROOTFS_POSTPROCESS_COMMAND += "do_alter_dates; "

OBMC_KERNEL_MODULES = " \
  kernel-module-ip6table-filter \
  kernel-module-ip6table-mangle \
  kernel-module-ip6-tables \
  kernel-module-ip6t-ipv6header \
  kernel-module-ip6t-reject \
  kernel-module-iptable-filter \
  kernel-module-iptable-mangle \
  kernel-module-iptable-nat \
  kernel-module-ip-tables \
  kernel-module-ipt-reject \
  kernel-module-rng-core \
  kernel-module-timeriomem-rng \
  kernel-module-x-tables \
  kernel-module-xt-addrtype \
  kernel-module-xt-conntrack \
  kernel-module-xt-log \
  kernel-module-xt-mark \
  kernel-module-xt-masquerade \
  kernel-module-xt-nat \
  kernel-module-xt-nflog \
  kernel-module-xt-state \
  kernel-module-xt-tcpmss \
  kernel-module-xt-tcpudp \
  "

OBMC_IMAGE_EXTRA_INSTALL_append = " \
  ${OBMC_KERNEL_MODULES} \
  active-gpio-on \
  allow-host-boot \
  profile-d \
  openssh \
  buttonscripts \
  glibc-localedata-en-us \
  glibc-localedata-ru-ru \
  glibc-binary-localedata-en-us \
  glibc-binary-localedata-ru-ru \
  pv \
  wget \
  firmware-updater \
  i2c-tools \
  rawi2ctool \
  net-initial \
  glibc-utils \
  mc mc-fish mc-helpers mc-helpers-perl mc-locale-ru \
  overheatd \
  reimu-conf \
  autofs \
  htop \
  mcst-fruid \
  astclkinfo \
  nfs-utils-client \
  nss-pam-ldapd \
  sysstat \
  "

IMAGE_INSTALL_remove += " \
mcst-fruid \
net-initial \
"

IMAGE_FEATURES_remove = "ssh-server-dropbear"
IMAGE_FEATURES_remove = "phosphor-state-manager-chassis"
