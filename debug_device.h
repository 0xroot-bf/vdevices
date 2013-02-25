#ifndef DEBUG_H_1727_INCLUDED
#define DEBUG_H_1727_INCLUDED

/* Number of devices to create (default: debug0 and debug1) */
#ifndef DEBUG_NDEVICES
#define DEBUG_NDEVICES 2    
#endif

/* Size of a buffer used for data storage */
#ifndef DEBUG_BUFFER_SIZE
#define DEBUG_BUFFER_SIZE 4000
#endif

/* Maxumum length of a block that can be read or written in one operation */
#ifndef DEBUG_BLOCK_SIZE
#define DEBUG_BLOCK_SIZE 512
#endif

struct debug_dev {
	unsigned char *data;
	unsigned long buffer_size; 
	unsigned long block_size;  
	struct mutex debug_mutex; 
	struct cdev cdev;
};
#endif