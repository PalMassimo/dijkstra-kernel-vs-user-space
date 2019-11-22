
mkdir Release
cd Release

gcc -std=c99 -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"sample.d" -MT"sample.o" -o "sample.o" "../sample.c"
gcc  -o "userspace-example"  ./sample.o   


