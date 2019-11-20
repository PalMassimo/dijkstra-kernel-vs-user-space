/**
 * @file   dijkstracharmutex.c
 * @author Marco Tessarotto
 * @date   19 September 2019
 * @version 0.1
 * @brief  An introductory character driver to support Dijkstra shortest path algorithm in kernel space. 
 * This module maps to /dev/dijkstrachar. This version has mutex locks to deal with synchronization problems.
 *  
*/

#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/fs.h>             // Header for the Linux file system support
//#include <asm/uaccess.h>          // Required for the copy to user function
#include <linux/uaccess.h>  /// scrivere per cosa serve

#include <linux/slab.h> ////

#include<linux/moduleparam.h>

#include <linux/ioctl.h>

#include <linux/mutex.h>	  // Required for the mutex functionality

// https://kernelnewbies.org/InternalKernelDataTypes
#include <linux/types.h>
//#include <inttypes.h>

#include <linux/kdev_t.h>
#include <linux/cdev.h>

//#include <linux/timekeeping.h>
#include <linux/timex.h>

#include <linux/mm.h> // kvmalloc_node

// https://www.kernel.org/doc/Documentation/scheduler/completion.txt
#include <linux/completion.h> //


// https://embetronicx.com/tutorials/linux/device-drivers/ioctl-tutorial-in-linux/

// https://stackoverflow.com/questions/2264384/how-do-i-use-ioctl-to-manipulate-my-kernel-module

//////////////// START of DIJKSTRA SECTION

typedef struct peer {
	u32 id;
	u32 distance;
} Peer;

typedef struct node {
	u32 visited; // si potrebbe usare uint16_t
	u32 num_adjacents; // si potrebbe usare uint16_t
	Peer * adjacent;
	u32 distance; // calculated by dijkstra algorithm
	u32 prev_node_id; // calculated by dijkstra algorithm
} Node;


#define NO_NODE_ID 0xFFFFFFFF

static __u32 origin_id;
static __u32 num_nodes = NO_NODE_ID;
static __u32 current_node_id = NO_NODE_ID;

static Node * nodes = NULL;

static char * dijkstra_output_buffer = NULL;

//////////////// END of DIJKSTRA SECTION

#define NUMA_NODE 0

#define NAME "dijkstrasp"
#define  DEVICE_NAME "dijkstrasp"    ///< The device will appear at /dev/dijkstrasp using this value
#define  CLASS_NAME  "dijkstrasp"        ///< The device class -- this is a character device driver

MODULE_LICENSE("GPL");            ///< The license type -- this affects available functionality
MODULE_AUTHOR("Marco Tessarotto");    ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A simple Linux char driver for Dijkstra shortest path algorithm ");  ///< The description -- see modinfo
MODULE_VERSION("0.1");            ///< A version number to inform users


//#define DIJKSTRA_MAJOR 0   /* dynamic major by default */
static int major = -1;
static struct cdev mycdev;
static struct class *myclass = NULL;

//
//dev_t dev;
//struct cdev cdev;
//
//static int    dijkstra_major = DIJKSTRA_MAJOR;      ///< Store the device number -- determined automatically
static char   message[256] = {0};           ///< Memory for the string that is passed from userspace
static short  size_of_message;              ///< Used to remember the size of the string stored
static int    numberOpens = 0;              ///< Counts the number of times the device is opened
//static struct class*  dijkstracharClass  = NULL; ///< The device-driver class struct pointer
//static struct device* dijkstracharDevice = NULL; ///< The device-driver device struct pointer
static u32 received_bytes;


static DEFINE_MUTEX(dijkstrachar_mutex);	    ///< Macro to declare a new mutex

/// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static long etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static void free_nodes(void);


#define WR_ORIGIN_NODE_ID _IOW('a','a',__s32*)
#define RD_ORIGIN_NODE_ID _IOR('a','b',__s32*)

#define WR_NUM_NODES _IOW('a','c',__s32*)
#define RD_NUM_NODES _IOR('a','d',__s32*)

#define SET_CURRENT_NODE_ID _IOR('a','e',__s32*)
#define GET_CURRENT_NODE_ID _IOR('a','f',__s32*)

#define START_DIJKSTRA_THREAD 0xDEADBEEF

// https://www.kernel.org/doc/Documentation/scheduler/completion.txt
struct completion {
	unsigned int done;
	wait_queue_head_t wait;
};


struct completion wait_for_dijkstra_completion;

//////////////// START of DIJKSTRA SECTION

void dijkstra_kernel_thread(void) {

	unsigned long long t1, t2;
	struct timespec start, stop;

	u32 unvisited=num_nodes;
	u32 indice;
	u32 min_distance;
	u32 i;
	u32 * pos;

	Peer * visit;

	if (num_nodes == NO_NODE_ID) {
		printk(KERN_INFO "dijkstra_kernel_thread: nothing to do!\n");
		return;
	}

	// https://stackoverflow.com/questions/22579157/kernel-mode-clock-gettime
	// see linux/timekeeping.h
	// see https://www.kernel.org/doc/html/latest/core-api/timekeeping.html


	// https://vincent.bernat.ch/en/blog/2017-linux-kernel-microbenchmark

	// https://elixir.bootlin.com/linux/latest/source/arch/x86/include/asm/msr.h#L201
	// works for micro-benchmarking (measures cpu ctcles, not timing)
	// https://stackoverflow.com/a/19942784/974287

	t1 = get_cycles();

	// void getnstimeofday (struct timespec *tv)

	// https://stackoverflow.com/a/4655754/974287
	// http://lxr.linux.no/#linux+v2.6.22.14/kernel/time.c#L310


	//	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);

	nodes[origin_id].distance=0;
	nodes[origin_id].prev_node_id = origin_id;

	while (unvisited) {

		min_distance = UINT_MAX;

		for(i = 0; i < num_nodes; i++) {
			if (nodes[i].distance < min_distance && nodes[i].visited == 0) {
				min_distance = nodes[i].distance;
				indice = i;
			}
		}

		if(min_distance == UINT_MAX)
			break;

		visit = nodes[indice].adjacent;

		for(i = 0; i < nodes[indice].num_adjacents; i++) {

			if(nodes[visit->id].distance > nodes[indice].distance + visit->distance) {
				nodes[visit->id].distance = nodes[indice].distance + visit->distance;
				nodes[visit->id].prev_node_id = indice;
			}

			visit++;
		}

		nodes[indice].visited = 1;

		unvisited--;
	}

	t2 = get_cycles();

	printk(KERN_INFO "dijkstra_kernel_thread: total cycles: %llu\n", t2 - t1);

	// node id, distance, id of preceding node
	dijkstra_output_buffer = kvmalloc_node(num_nodes * 3 * sizeof(u32), GFP_KERNEL, NUMA_NODE);

	pos = (u32 *) dijkstra_output_buffer;

	// write results in a dijkstra_output_buffer

	for (i = 0; i< num_nodes; i++) {
		*pos++ = i;
		*pos++ = nodes[i].distance;
		*pos++ = nodes[i].prev_node_id;
	}

	complete(&wait_for_dijkstra_completion);

//	fprintf(stdout, "[kernel_process] dijkstra finished \n");
}

//////////////// END of DIJKSTRA SECTION

/*
 * notes:
 *
 *sudo mknod  /dev/dijkstrasp c 1 0 <<---- does not work!
 *
 *
 *
 * sudo insmod dijkstra.ko
 * sudo chmod ugo+rwx /dev/dijkstrasp
 * ../lettore_grafo_lkm/Debug/lettore_grafo_lkm
 *
 * sudo rmmod dijkstra
 *
 */

/**
 * Devices are represented as file structure in the kernel. The file_operations structure from
 * /linux/fs.h lists the callback functions that you wish to associated with your file operations
 * using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
// https://elixir.bootlin.com/linux/v5.3.8/source/include/linux/fs.h#L1789
static struct file_operations fops =
{  .owner = THIS_MODULE,
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
   .unlocked_ioctl = etx_ioctl,
};

//static struct cdev etx_cdev;

// https://embetronicx.com/tutorials/linux/device-drivers/cdev-structure-and-file-operations-of-character-drivers/#Example

// https://linux-kernel-labs.github.io/master/labs/device_model.html
//

// https://cirosantilli.com/linux-kernel-module-cheat#automatically-create-character-device-file-on-insmod
static void cleanup(int device_created)
{
	if (device_created) {
		device_destroy(myclass, major);
		cdev_del(&mycdev);
	}

	if (myclass)
		class_destroy(myclass);

	if (major != -1)
		unregister_chrdev_region(major, 1);

}

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init myinit(void)
{
	int device_created = 0;

	/* cat /proc/devices */
	/*
	 * Register your major, and accept a dynamic number.
	 */
	/* output is written to major */
	if (alloc_chrdev_region(&major, 0, 1, NAME "_proc") < 0) {
		printk(KERN_ALERT "dijkstrachar: failed to register a major number\n");
		goto error;
	}

	printk(KERN_INFO "dijkstrachar: registered correctly with major number %d\n", MAJOR(major));
	/* ls /sys/class */
	if ((myclass = class_create(THIS_MODULE, NAME "_sys")) == NULL)
		goto error;

	// Register the device driver
	/* ls /dev/ */
	if (device_create(myclass, NULL, major, NULL, NAME "_dev") == NULL)
		goto error;

	device_created = 1;

	cdev_init(&mycdev, &fops);

	if (cdev_add(&mycdev, major, 1) == -1)
		goto error;

	mutex_init(&dijkstrachar_mutex);          // Initialize the mutex dynamically

    return 0;

error:
	cleanup(device_created);
	return -1;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit myexit(void)
{
	mutex_destroy(&dijkstrachar_mutex);                       // destroy the dynamically-allocated mutex

	cleanup(1);

	printk(KERN_INFO "dijkstrachar: goodbye from the LKM!\n");
}


/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep) {

   if(!mutex_trylock(&dijkstrachar_mutex)){                  // Try to acquire the mutex (returns 0 on fail)
	printk(KERN_ALERT "dijkstrachar: device in use by another process");
	return -EBUSY;
   }
   numberOpens++;
   printk(KERN_INFO "dijkstrachar: device has been opened %d time(s)\n", numberOpens);
   return 0;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
   int error_count = 0;

   // https://www.kernel.org/doc/Documentation/scheduler/completion.txt
   wait_for_completion(&wait_for_dijkstra_completion);


   // copy_to_user has the format ( * to, *from, size) and returns 0 on success
   error_count = copy_to_user(buffer, message, size_of_message);

   if (error_count==0){           // success!
      printk(KERN_INFO "dijkstrachar: sent %d characters to the user\n", size_of_message);
      return (size_of_message=0); // clear the position to the start and return 0
   }
   else {
      printk(KERN_INFO "dijkstrachar: failed to send %d characters to the user\n", error_count);
      return -EFAULT;      // Failed -- return a bad address message (i.e. -14)
   }
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using message[x] = buffer[x]
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {

	char *kern_buf;
	u32 node_id;
	u32 num_adjacents;
	u32 i;
	u32 calculated_len;
	Node * n;
	u32 * buf;

	printk(KERN_INFO "dijkstrachar dev_write: len=%lu bytes\n", len);

	if (/*current_node_id == NO_NODE_ID ||*/ num_nodes == NO_NODE_ID /*|| current_node_id >= num_nodes*/) {
		printk(KERN_INFO "dijkstrachar: wrong num_nodes=%u\n", num_nodes);
		return -EINVAL;
	}

    /* Allocate memory in kernel */
    kern_buf = kvmalloc_node(len, GFP_KERNEL, NUMA_NODE);
    /*
     * The kmalloc() function returns physically and therefore virtually contiguous memory.
     * This is a contrast to user space's malloc() function, which returns virtually but not necessarily
     * physically contiguous memory. Physically contiguous memory has two primary benefits. First,
     * many hardware devices cannot address virtual memory. Therefore, in order for them to be able to
     * access a block of memory, the block must exist as a physically contiguous chunk of memory.
     * Second, a physically contiguous block of memory can use a single large page mapping.
     * This minimizes the translation lookaside buffer (TLB) overhead of addressing the memory,
     * as only a single TLB entry is required.
     */
    // https://www.kernel.org/doc/htmldocs/kernel-hacking/routines-kmalloc.html
    // https://www.kernel.org/doc/htmldocs/kernel-api/API-kmalloc.html
    // kmalloc is the normal method of allocating memory for objects smaller than page size in the kernel
    if (!kern_buf)
      return -ENOMEM;

    /* Transfer data from user to kernel through kernel buffer*/
    if(copy_from_user(kern_buf,buffer,len)) // kern_buf is destination, buffer is source; length in bytes
    {
    	printk(KERN_INFO "dijkstrachar: copy_from_user error error\n");
        kvfree(kern_buf);
        return -EFAULT;
    }

    if (len == 4 && *((u32 *) kern_buf) == START_DIJKSTRA_THREAD) {
    	printk(KERN_INFO "dijkstrachar: START_DIJKSTRA_THREAD\n");

    	for (i = 0; i < 100; i++)
    		dijkstra_kernel_thread();

    	return len;
    }


	// expected length (in bytes) : sizeof(u32) * 2 + sizeof(u32) * num_adj
	printk(KERN_INFO "dijkstrachar: minimum expected length : %lu bytes\n", sizeof(u32) * 2);

	if (len < sizeof(u32) * 2) { // miminum len in bytes
		printk(KERN_INFO "dijkstrachar: wrong len= %lu bytes\n", len);
		kvfree(kern_buf);
		return -EINVAL;
	}

//    for (i = 0; i < len; i++) {
//    	printk(KERN_INFO "kern_buf[%u] = %u\n", i, kern_buf[i]);
//    }

	received_bytes += len;

    buf = (u32 *) kern_buf;

    // read from userspace data for current node
    node_id = buf[0];

    printk(KERN_INFO "dijkstrachar: node_id=%u\n", node_id);

    if (node_id >= num_nodes) {
		printk(KERN_INFO "dijkstrachar: wrong node_id= %u, max valid value is %u\n", node_id, num_nodes-1);
		kvfree(kern_buf);
		return -EINVAL;
    }

//    peer_id = buf[1];
    num_adjacents = buf[1];

    printk(KERN_INFO "dijkstrachar: num_adjacents=%u\n", num_adjacents);

    calculated_len = sizeof(u32) * (2 + num_adjacents * 2);

    printk(KERN_INFO "dijkstrachar: calculated_len=%u\n", calculated_len);

    if (len < calculated_len)
    {
    	printk(KERN_INFO "dijkstrachar: wrong len= %lu, expected %u\n", len, calculated_len);
        kvfree(kern_buf);
        return -EFAULT;
    }

    // read node data

    n = &nodes[node_id];

    n->num_adjacents = num_adjacents;
    n->adjacent = num_adjacents > 0 ? kvmalloc_node(sizeof(Peer) * num_adjacents, GFP_KERNEL, NUMA_NODE) : NULL;

    n->distance = 0;
    n->visited = 0;
    n->prev_node_id = NO_NODE_ID;

    for (i = 0; i < num_adjacents; i++) {
    	n->adjacent[i].id = buf[2 + i * 2];
    	n->adjacent[i].distance = buf[2 + i * 2 + 1];

    	printk(KERN_INFO "adjacent i=%u - id=%u, distance=%u\n", i, n->adjacent[i].id, n->adjacent[i].distance);

    	if (n->adjacent[i].id >= num_nodes) {
    		printk(KERN_INFO "dijkstrachar: wrong adjacent id= %u\n", n->adjacent[i].id);
            kvfree(kern_buf);
            return -EFAULT;
    	}
    }

	kvfree(kern_buf);

	printk(KERN_INFO "dijkstrachar: completed read of node %u\n", node_id);

//   sprintf(message, "%s(%zu letters)", buffer, len);   // appending received string with its length
//   size_of_message = strlen(message);                 // store the length of the stored message
//   printk(KERN_INFO "dijkstrachar: Received %zu characters from the user\n", len);

//	printk(KERN_INFO "dijkstrachar: num_nodes =%u origin_id=%u\n", num_nodes, origin_id);



   return len;
}


static void free_nodes() {
	u32 i;

	if (nodes == NULL)
		return;

	for (i = 0; i < num_nodes; i++) {
		if (nodes[i].adjacent != NULL)
			kvfree(nodes[i].adjacent);
	}

}

static long etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int res;

	 switch(cmd) {
			case WR_ORIGIN_NODE_ID:
					res = copy_from_user(&origin_id ,(__u32*) arg, sizeof(origin_id));

					if (res != 0) printk(KERN_INFO "etx_ioctl: copy_from_user error - WR_ORIGIN_NODE_ID\n");

					printk(KERN_INFO "etx_ioctl: origin_id = %d\n", origin_id);
					break;
			case RD_ORIGIN_NODE_ID:
					res = copy_to_user((__u32*) arg, &origin_id, sizeof(origin_id));

					if (res != 0) printk(KERN_INFO "etx_ioctl: copy_to_user error - RD_ORIGIN_NODE_ID\n");

					break;
			case WR_NUM_NODES:
					if (nodes != NULL) {
						// TODO: check for dijstra in execution
						printk(KERN_INFO "dijkstrachar, etx_ioctl: kvfree nodes\n");

						// https://www.kernel.org/doc/html/latest/core-api/mm-api.html#c.kvmalloc_node
						kvfree(nodes);

					}
					res = copy_from_user(&num_nodes ,(__u32*) arg, sizeof(num_nodes));

					if (res != 0) printk(KERN_INFO "etx_ioctl: copy_from_user error - WR_NUM_NODES\n");

					printk(KERN_INFO "etx_ioctl: num_nodes = %d\n", num_nodes);

					// https://www.linuxjournal.com/article/6930
					// kmalloc() function returns physically and therefore virtually contiguous memory
					//
//					nodes = kmalloc(sizeof(Node) * num_nodes, GFP_KERNEL);

					// https://www.kernel.org/doc/html/latest/vm/numa.html
					// https://www.kernel.org/doc/html/latest/core-api/mm-api.html#c.kvmalloc_node
					nodes = kvmalloc_node(sizeof(Node) * num_nodes, GFP_KERNEL, NUMA_NODE /* NUMA node*/);

					printk(KERN_INFO "etx_ioctl: after kmalloc %ld bytes\n", sizeof(Node) * num_nodes);

					printk(KERN_INFO "etx_ioctl: sizeof(Node) = %lu\n", sizeof(Node));

					memset(nodes, 0, sizeof(Node) * num_nodes);

					received_bytes = 0;

					break;
			case RD_NUM_NODES:
					res = copy_to_user((__u32*) arg, &num_nodes, sizeof(num_nodes));

					if (res != 0) printk(KERN_INFO "etx_ioctl: copy_to_user error - RD_NUM_NODES\n");

					break;
			case SET_CURRENT_NODE_ID:
					res = copy_from_user(&current_node_id,(__u32*) arg, sizeof(current_node_id));

					if (res != 0) printk(KERN_INFO "etx_ioctl: copy_from_user error - SET_CURRENT_NODE_ID\n");

					break;
			case GET_CURRENT_NODE_ID:
					res = copy_to_user((__u32*) arg, &current_node_id, sizeof(current_node_id));

					if (res != 0) printk(KERN_INFO "etx_ioctl: copy_to_user error - GET_CURRENT_NODE_ID\n");

					break;
	}
	return 0;
}


/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep) {

	if (nodes != NULL) {
		// TODO: check for dijstra during execution
		printk(KERN_INFO "dijkstrachar, dev_release: kvfree nodes\n");
		free_nodes();
		kvfree(nodes);
		nodes = NULL;
		origin_id = 0;
	}

	if (dijkstra_output_buffer != NULL) {
		kvfree(dijkstra_output_buffer);
		dijkstra_output_buffer = NULL;
	}

	num_nodes = current_node_id = NO_NODE_ID;

	mutex_unlock(&dijkstrachar_mutex);                      // release the mutex (i.e., lock goes up)

	printk(KERN_INFO "dijkstrachar: device successfully closed\n");

	return 0;
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
//

module_init(myinit)
module_exit(myexit)


