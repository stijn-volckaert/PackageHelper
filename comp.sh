PHVERSION="v14"

rm PackageHelper_${PHVERSION}.o
rm PackageHelper_${PHVERSION}.so
rm ../System/PackageHelper_${PHVERSION}.so

# compile and output to this folder -> no linking yet!
gcc-2.95  -c -D__LINUX_X86__ -fno-for-scope -O2 -fomit-frame-pointer -march=pentium -D_REENTRANT \
-DGPackage=PackageHelper_${PHVERSION} -Werror -I./Inc -I../Core/Inc -I../Engine/Inc \
-o./PackageHelper_${PHVERSION}.o Src/PackageHelper.cpp

# link with UT libs
gcc-2.95  -shared -o PackageHelper_${PHVERSION}.so -Wl,-rpath,. \
-nodefaultlibs -lgcc \
-export-dynamic \
PackageHelper_${PHVERSION}.o \
~/UTLinux/System436b/Core.so ~/UTLinux/System436b/Engine.so

# copy
cp PackageHelper_${PHVERSION}.so ../System/
cp PackageHelper_${PHVERSION}.so ../System451/
