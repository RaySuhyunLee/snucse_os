%define config_name artik10_defconfig
%define buildarch arm
%define target_board artik10
%define variant %{buildarch}-%{target_board}
%define dtb_name exynos5422-artik10.dtb

Name: artik10-linux-kernel
Summary: The Linux Kernel for ARTIK10
Version: 3.10.93
Release: 0
License: GPL-2.0
ExclusiveArch: %{arm}
Group: System/Kernel
Vendor: The Linux Community
URL: http://www.kernel.org
Source0:   linux-kernel-%{version}.tar.xz
BuildRoot: %{_tmppath}/%{name}-%{PACKAGE_VERSION}-root

%define fullVersion %{version}-%{release}-%{variant}

BuildRequires: module-init-tools
BuildRequires: bc

%description
The Linux Kernel, the operating system core itself

%package -n %{variant}-linux-kernel
Summary: Tizen kernel for %{target_board}
Group: System/Kernel
# Create more provides-profiles if this supports other profiles
Provides: %{variant}-artik10-kernel-profile-common = %{version}-%{release}
Provides: %{variant}-kernel-uname-r = %{fullVersion}

%description -n %{variant}-linux-kernel
This package contains the Linux kernel for Tizen (common profile, arch %{buildarch}, target board %{target_board})

%package -n %{variant}-linux-kernel-modules
Summary: Kernel modules for %{target_board}
Group: System/Kernel
Provides: %{variant}-kernel-modules = %{fullVersion}
Provides: %{variant}-kernel-modules-uname-r = %{fullVersion}

%description -n %{variant}-linux-kernel-modules
Kernel-modules includes the loadable kernel modules(.ko files) for %{target_board}

%prep
%setup -q -n linux-kernel-%{version}

%build
# Make sure EXTRAVERSION says what we want it to say
sed -i "s/^EXTRAVERSION.*/EXTRAVERSION = -%{release}-%{variant}/" Makefile

# 1. Compile sources
make %{config_name}
make %{?_smp_mflags}

# 2. Build zImage
make zImage %{?_smp_mflags}
make %{dtb_name} %{?_smp_mflags}

# 3. Build modules
make modules %{?_smp_mflags}

%install
QA_SKIP_BUILD_ROOT="DO_NOT_WANT"; export QA_SKIP_BUILD_ROOT

# 1. Destynation directories
mkdir -p %{buildroot}/boot/
mkdir -p %{buildroot}/lib/modules/%{fullVersion}

# 2. Install zImage, System.map, ...
cat arch/arm/boot/zImage arch/arm/boot/dts/%{dtb_name} > arch/arm/boot/zImage-dtb
install -m 755 arch/arm/boot/zImage-dtb %{buildroot}/boot/zImage
install -m 644 arch/arm/boot/dts/*.dtb %{buildroot}/boot/

install -m 644 System.map %{buildroot}/boot/System.map-%{fullVersion}
install -m 644 .config %{buildroot}/boot/config-%{fullVersion}

# 3. Install modules
make INSTALL_MOD_STRIP=1 INSTALL_MOD_PATH=%{buildroot} modules_install KERNELRELEASE=%{fullVersion}

rm -rf %{buildroot}/boot/vmlinux*
rm -rf %{buildroot}/System.map*
rm -rf %{buildroot}/vmlinux*
rm -rf %{buildroot}/lib/firmware

# 7. Update file permisions
find %{buildroot}/lib/modules/ -name "*.ko"                                     -type f -exec chmod 755 {} \;

# 8. Create symbolic links
rm -f %{buildroot}/lib/modules/%{fullVersion}/build
rm -f %{buildroot}/lib/modules/%{fullVersion}/source

%clean
rm -rf %{buildroot}

%files -n %{variant}-linux-kernel-modules
/lib/modules/

%files -n %{variant}-linux-kernel
%license COPYING
/boot/zImage
/boot/%{dtb_name}
/boot/System.map*
/boot/config*
