# timerService
It's a timer service used for linux. A thread to listen new timer added and wait for minimum timer expire. All timer are added into a timer event table that are organized as a left binary heap.
HOW TO:
make
make -f Makefile.test
./timerService_test
