/* vialab.h */

#ifndef VIALAB_H_1727_INCLUDED
#define VIALAB_H_1727_INCLUDED

/* Number of devices to create (default: vialab0 and vialab1) */
#ifndef VIALAB_NDEVICES
#define VIALAB_NDEVICES 2    
#endif

/* Size of a buffer used for data storage */
#ifndef VIALAB_BUFFER_SIZE
#define VIALAB_BUFFER_SIZE 4000
#endif

/* Maxumum length of a block that can be read or written in one operation */
#ifndef VIALAB_BLOCK_SIZE
#define VIALAB_BLOCK_SIZE 512
#endif

/* The structure to represent 'vialab' devices. 
 *  data - data buffer;
 *  buffer_size - size of the data buffer;
 *  block_size - maximum number of bytes that can be read or written 
 *    in one call;
 *  vialab_mutex - a mutex to protect the fields of this structure;
 *  cdev - ñharacter device structure.
 */
struct vialab_dev {
	unsigned char *data;
	unsigned long buffer_size; 
	unsigned long block_size;  
	struct mutex vialab_mutex; 
	struct cdev cdev;
};
#endif /* VIALAB_H_1727_INCLUDED */
