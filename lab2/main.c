//
// Created by kain on 17.03.2021.
//

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/string.h>

#include "lab2.h"

MODULE_AUTHOR("Eugene and Eridan");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BLOCK DRIVER");
MODULE_VERSION("1.0");

static int my_open(struct block_device *x, fmode_t mode)
{
    int ret=0;
    printk(KERN_INFO "mydiskdrive : open \n");

    return ret;

}

static void my_release(struct gendisk *disk, fmode_t mode)
{
    printk(KERN_INFO "mydiskdrive : closed \n");
}

tatic struct block_device_operations fops =
        {
                .owner = THIS_MODULE,
                .open = my_open,
                .release = my_release,
        };

int mydisk_init(void)
{
    (device.data) = vmalloc(MEMSIZE * SECTOR_SIZE);
    /* Setup its partition table */
    copy_mbr_n_br(device.data);

    return MEMSIZE;
}

static int rb_transfer(struct request *req)
{
    int dir = rq_data_dir(req);
    int ret = 0;
    /*starting sector
     *where to do operation*/
    sector_t start_sector = blk_rq_pos(req);
    unsigned int sector_cnt = blk_rq_sectors(req); /* no of sector on which opn to be done*/
    struct bio_vec bv;
#define BV_PAGE(bv) ((bv).bv_page)
#define BV_OFFSET(bv) ((bv).bv_offset)
#define BV_LEN(bv) ((bv).bv_len)
    struct req_iterator iter;
    sector_t sector_offset;
    unsigned int sectors;
    u8 *buffer;
    sector_offset = 0;
    rq_for_each_segment(bv, req, iter)
    {
        buffer = page_address(BV_PAGE(bv)) + BV_OFFSET(bv);
        if (BV_LEN(bv) % (SECTOR_SIZE) != 0)
        {
            printk(KERN_ERR"bio size is not a multiple ofsector size\n");
            ret = -EIO;
        }
        sectors = BV_LEN(bv) / SECTOR_SIZE;
        printk(KERN_DEBUG "my disk: Start Sector: %llu, Sector Offset: %llu;\
		Buffer: %p; Length: %u sectors\n",\
		(unsigned long long)(start_sector), (unsigned long long) \
		(sector_offset), buffer, sectors);

        if (dir == WRITE) /* Write to the device */
        {
            memcpy((device.data)+((start_sector+sector_offset)*SECTOR_SIZE)\
			,buffer,sectors*SECTOR_SIZE);
        }
        else /* Read from the device */
        {
            memcpy(buffer,(device.data)+((start_sector+sector_offset)\
			*SECTOR_SIZE),sectors*SECTOR_SIZE);
        }
        sector_offset += sectors;
    }

    if (sector_offset != sector_cnt)
    {
        printk(KERN_ERR "mydisk: bio info doesn't match with the request info");
        ret = -EIO;
    }
    return ret;
}
/** request handling function**/
static void dev_request(struct request_queue *q)
{
    struct request *req;
    int error;
    while ((req = blk_fetch_request(q)) != NULL) /*check active request
						      *for data transfer*/
    {
        error=rb_transfer(req);// transfer the request for operation
        __blk_end_request_all(req, error); // end the request
    }
}

void device_setup(void)
{
    mydisk_init();
    c = register_blkdev(c, "mydisk");// major no. allocation
    printk(KERN_ALERT "Major Number is : %d",c);
    spin_lock_init(&device.lock); // lock for queue
    device.queue = blk_init_queue( dev_request, &device.lock);

    device.gd = alloc_disk(8); // gendisk allocation

    (device.gd)->major=c; // major no to gendisk
    device.gd->first_minor=0; // first minor of gendisk

    device.gd->fops = &fops;
    device.gd->private_data = &device;
    device.gd->queue = device.queue;
    device.size= mydisk_init();
    printk(KERN_INFO"THIS IS DEVICE SIZE %d",device.size);
    sprintf(((device.gd)->disk_name), "mydisk");
    set_capacity(device.gd, device.size);
    add_disk(device.gd);
}

static int __init mydiskdrive_init(void)
{
    int ret=0;
    device_setup();

    return ret;
}

void mydisk_cleanup(void)
{
    vfree(device.data);
}

void __exit mydiskdrive_exit(void)
{
    del_gendisk(device.gd);
    put_disk(device.gd);
    blk_cleanup_queue(device.queue);
    unregister_blkdev(c, "mydisk");
    mydisk_cleanup();
}

module_init(mydiskdrive_init);
module_exit(mydiskdrive_exit);
