# Multithreaded-RLE

A simple multi-threaded run length encoder built to speed up the compression of large data files. The encoder breaks up the file into 4 mb chunks that is then fed into a task 
queue. A worker pool is created and will synchronously create tasks at the same time as worker threads performs the tasks in the queue, with the results being stitched together 
synchronously as well. Shared resources are protected with mutexes and condition variables and project is thread safe to ensure protection from deadlocks and race conditions.

# Usage

./encode file.txt > compressed.txt 

This will encode a single file using one thread. 

./encode file.txt file2.txt > compressed.txt 

If multiple files are provided, they will be concatenated and compressed as a single file.

./encode -j 3 file.txt > compressed.txt 

The -j flag specifies how many threads you want to use to run the encoder. 
