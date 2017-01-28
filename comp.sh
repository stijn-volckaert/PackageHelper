rm PackageHelper_v13.o
rm PackageHelper_v13.so
rm ../System/PackageHelper_v13.so

# compile and output to this folder -> no linking yet!
gcc-2.95  -c -D__LINUX_X86__ -fno-for-scope -O2 -fomit-frame-pointer -march=pentium -D_REENTRANT \
-DGPackage=PackageHelper_v13 -Werror -I../PackageHelper_v13/Inc -I../Core/Inc -I../Engine/Inc \
-o../PackageHelper_v13/PackageHelper_v13.o Src/PackageHelper.cpp

# link with UT libs
gcc-2.95  -shared -o PackageHelper_v13.so -Wl,-rpath,. \
-nodefaultlibs -lgcc \
-export-dynamic \
PackageHelper_v13.o \
~/UTLinux/System436b/Core.so ~/UTLinux/System436b/Engine.so

# copy
cp PackageHelper_v13.so ../System/
cp PackageHelper_v13.so ../System451/
