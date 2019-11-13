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



// https://embetronicx.com/tutorials/linux/device-drivers/ioctl-tutorial-in-linux/

// https://stackoverflow.com/questions/2264384/how-do-i-use-ioctl-to-manipulate-my-kernel-module

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

#define  DEVICE_NAME "dijkstrasp"    ///< The device will appear at /dev/dijkstrasp using this value
#define  CLASS_NAME  "dijkstrasp"        ///< The device class -- this is a character device driver

MODULE_LICENSE("GPL");            ///< The license type -- this affects available functionality
MODULE_AUTHOR("Marco Tessarotto");    ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A simple Linux char driver for Dijkstra shortest path algorithm ");  ///< The description -- see modinfo
MODULE_VERSION("0.1");            ///< A version number to inform users

static int    majorNumber;                  ///< Store the device number -- determined automatically
static char   message[256] = {0};           ///< Memory for the string that is passed from userspace
static short  size_of_message;              ///< Used to remember the size of the string stored
static int    numberOpens = 0;              ///< Counts the number of times the device is opened
static struct class*  dijkstracharClass  = NULL; ///< The device-driver class struct pointer
static struct device* dijkstracharDevice = NULL; ///< The device-driver device struct pointer

static Node * nodes = NULL;

static DEFINE_MUTEX(dijkstrachar_mutex);	    ///< Macro to declare a new mutex

/// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static long etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

#define NO_NODE_ID 0xFFFFFFFF

static __u32 origin_id;
static __u32 num_nodes = NO_NODE_ID;
static __u32 current_node_id = NO_NODE_ID;

#define WR_ORIGIN_NODE_ID _IOW('a','a',__s32*)
#define RD_ORIGIN_NODE_ID _IOR('a','b',__s32*)

#define WR_NUM_NODES _IOW('a','c',__s32*)
#define RD_NUM_NODES _IOR('a','d',__s32*)

#define SET_CURRENT_NODE_ID _IOR('a','e',__s32*)
#define GET_CURRENT_NODE_ID _IOR('a','f',__s32*)

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

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init dijkstrachar_init(void){
   printk(KERN_INFO "dijkstrachar: Initializing the dijkstrachar LKM\n");

   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   // dynamic allocation of major number
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "dijkstrachar failed to register a major number\n");
      return majorNumber;
   }
   printk(KERN_INFO "dijkstrachar: registered correctly with major number %d\n", majorNumber);

//   /*Creating cdev structure*/
//   cdev_init(&etx_cdev,&fops);
//
//   /*Adding character device to the system*/
//   if((cdev_add(&etx_cdev,dev,1)) < 0){
//        printk(KERN_INFO "Cannot add the device to the system\n");
//        goto r_class;
//    }

   // Register the device class
   // create device class in sysfs
   dijkstracharClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(dijkstracharClass)){           // Check for error and clean up if there is
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to register device class\n");
      return PTR_ERR(dijkstracharClass);     // Correct way to return an error on a pointer
   }
   printk(KERN_INFO "dijkstrachar: device class registered correctly\n");

   // Register the device driver
   // create device under /dev/
   dijkstracharDevice = device_create(dijkstracharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(dijkstracharDevice)){          // Clean up if there is an error
      class_destroy(dijkstracharClass);      // Repeated code but the alternative is goto statements
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to create the device\n");
      return PTR_ERR(dijkstracharDevice);
   }
   printk(KERN_INFO "dijkstrachar: device class created correctly\n"); // Made it! device was initialized
   mutex_init(&dijkstrachar_mutex);          // Initialize the mutex dynamically
   return 0;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit dijkstrachar_exit(void){
   mutex_destroy(&dijkstrachar_mutex);                       // destroy the dynamically-allocated mutex

   printk(KERN_INFO "dijkstrachar: exit, before device_destroy\n");
   device_destroy(dijkstracharClass, MKDEV(majorNumber, 0)); // remove the device

   printk(KERN_INFO "dijkstrachar: exit, before class_unregister\n");
   class_unregister(dijkstracharClass);                      // unregister the device class

   printk(KERN_INFO "dijkstrachar: exit, before class_destroy\n");
   // kernel warning: refcount_t: underflow; use-after-free.
   class_destroy(dijkstracharClass);                         // remove the device class

//   printk(KERN_INFO "dijkstrachar: exit, before cdev_del\n");
//   cdev_del(&etx_cdev);

   printk(KERN_INFO "dijkstrachar: exit, before unregister_chrdev\n");
   unregister_chrdev(majorNumber, DEVICE_NAME);         // unregister the major number

   printk(KERN_INFO "dijkstrachar: Goodbye from the LKM!\n");
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep){

   if(!mutex_trylock(&dijkstrachar_mutex)){                  // Try to acquire the mutex (returns 0 on fail)
	printk(KERN_ALERT "dijkstrachar: Device in use by another process");
	return -EBUSY;
   }
   numberOpens++;
   printk(KERN_INFO "dijkstrachar: Device has been opened %d time(s)\n", numberOpens);
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
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   int error_count = 0;
   // copy_to_user has the format ( * to, *from, size) and returns 0 on success
   error_count = copy_to_user(buffer, message, size_of_message);

   if (error_count==0){           // success!
      printk(KERN_INFO "dijkstrachar: Sent %d characters to the user\n", size_of_message);
      return (size_of_message=0); // clear the position to the start and return 0
   }
   else {
      printk(KERN_INFO "dijkstrachar: Failed to send %d characters to the user\n", error_count);
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
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){

	char *kern_buf;
	u32 node_id;
	u32 peer_id;
	u32 num_adjacents;
	u32 i;
	u32 calculated_len;
	Node * n;

	printk(KERN_INFO "dijkstrachar: len= %lu\n", len);

	if (current_node_id == NO_NODE_ID || num_nodes == NO_NODE_ID || current_node_id >= num_nodes) {
		printk(KERN_INFO "dijkstrachar: wrong current_node_id= %u or num_nodes=%u\n", current_node_id, num_nodes);
		return -EINVAL;
	}

	// expected length: sizeof(u32) * 2 + sizeof(u32) * num_adj

	if (len < sizeof(u32) * 3) { // miminum len
		printk(KERN_INFO "dijkstrachar: wrong len= %lu\n", len);
		return -EINVAL;
	}

    /* Allocate memory in kernel */
    kern_buf = kmalloc (len, GFP_KERNEL);
    if (!kern_buf)
      return -ENOMEM;

    /* Transfer data from user to kernel through kernel buffer*/
    if(copy_from_user(kern_buf,buffer,len))
    {
        kfree(kern_buf);
        return -EFAULT;
    }

    u32 * buf = (u32 *) kern_buf;

    // read from userspace data for current node
    node_id = buf[0];
//    peer_id = buf[1];
    num_adjacents = buf[1];

    calculated_len = sizeof(u32) * (2 + num_adjacents * 2);

    if (len < calculated_len)
    {
    	printk(KERN_INFO "dijkstrachar: wrong len= %lu, expected %ly\n", len, calculated_len);
        kfree(kern_buf);
        return -EFAULT;
    }

    if (node_id >= num_nodes) {
    	printk(KERN_INFO "dijkstrachar: wrong node_id= %u\n", node_id);
        kfree(kern_buf);
        return -EFAULT;
    }

    // read node data

    n = &nodes[node_id];

    n->num_adjacents = num_adjacents;
    n->adjacent = num_adjacents > 0 ? kmalloc(sizeof(Peer) * num_adjacents, GFP_KERNEL) : NULL;

    n->distance = 0;
    n->visited = 0;
    n->prev_node_id = NO_NODE_ID;

    for (i = 0; i < num_adjacents; i++) {
    	n->adjacent[i].id = buf[2 + i * 2];
    	n->adjacent[i].distance = buf[2 + i * 2 + 1];

    	if (n->adjacent[i].id >= num_nodes) {
    		printk(KERN_INFO "dijkstrachar: wrong adjacent id= %u\n", n->adjacent[i].id);
            kfree(kern_buf);
            return -EFAULT;
    	}
    }

	kfree(kern_buf);

	printk(KERN_INFO "dijkstrachar: completed read of node %u\n", node_id);

//   sprintf(message, "%s(%zu letters)", buffer, len);   // appending received string with its length
//   size_of_message = strlen(message);                 // store the length of the stored message
//   printk(KERN_INFO "dijkstrachar: Received %zu characters from the user\n", len);

//	printk(KERN_INFO "dijkstrachar: num_nodes =%u origin_id=%u\n", num_nodes, origin_id);



   return len;
}

static long etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
         switch(cmd) {
                case WR_ORIGIN_NODE_ID:
                        copy_from_user(&origin_id ,(__u32*) arg, sizeof(origin_id));
                        printk(KERN_INFO "origin_id = %d\n", origin_id);
                        break;
                case RD_ORIGIN_NODE_ID:
                        copy_to_user((__u32*) arg, &origin_id, sizeof(origin_id));
                        break;
                case WR_NUM_NODES:
                		if (nodes != NULL) {
                			// TODO: check for dijstra in execution
                			printk(KERN_INFO "dijkstrachar, etx_ioctl: kfree nodes\n");
                			kfree(nodes);

                		}
                        copy_from_user(&num_nodes ,(__u32*) arg, sizeof(num_nodes));
                        printk(KERN_INFO "num_nodes = %d\n", num_nodes);

                        nodes = kmalloc(sizeof(Node) * num_nodes, GFP_KERNEL);

                        printk(KERN_INFO "after kmalloc %d bytes\n", sizeof(Node) * num_nodes);


                        break;
                case RD_NUM_NODES:
                        copy_to_user((__u32*) arg, &num_nodes, sizeof(num_nodes));
                        break;
                case SET_CURRENT_NODE_ID:
                		copy_from_user(&current_node_id,(__u32*) arg, sizeof(current_node_id));
                		break;
                case GET_CURRENT_NODE_ID:
                		copy_to_user((__u32*) arg, &current_node_id, sizeof(current_node_id));
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
		printk(KERN_INFO "dijkstrachar, dev_release: kfree nodes\n");
		kfree(nodes);

	}

	num_nodes = current_node_id = NO_NODE_ID;
	mutex_unlock(&dijkstrachar_mutex);                      // release the mutex (i.e., lock goes up)
	printk(KERN_INFO "dijkstrachar: Device successfully closed\n");
	return 0;
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
//module_init(dijkstrachar_init);
//module_exit(dijkstrachar_exit);

module_init(dijkstrachar_init);
module_exit(dijkstrachar_exit);
