It's a timer service used for linux. A thread to listen new timer added and wait for minimum timer expire. All timer are added into a timer event table that are organized as a left binary heap.    

complexity of binary heap tree and linked list:

                |       start       |       find        |       delete  
binary tree     |       O(log2n)    |       O(1)        |       O(2*log2n)
linked list     |       O(n/2)      |       O(n/2)      |       O(1)

HOW TO:    

make  

make -f Makefile.test    

./timerService_test
