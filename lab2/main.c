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
#include <linux/hdreg.h>


/* Size of Ram disk in sectors */
#define MEMSIZE 0x1B000

#define SECTOR_SIZE 512
#define MBR_SIZE SECTOR_SIZE
#define MBR_DISK_SIGNATURE_OFFSET 440
#define MBR_DISK_SIGNATURE_SIZE 4
#define PARTITION_TABLE_OFFSET 446
#define PARTITION_ENTRY_SIZE 16
#define PARTITION_TABLE_SIZE 64
#define MBR_SIGNATURE_OFFSET 510
#define MBR_SIGNATURE_SIZE 2
#define MBR_SIGNATURE 0xAA55
#define BR_SIZE SECTOR_SIZE
#define BR_SIGNATURE_OFFSET 510
#define BR_SIGNATURE_SIZE 2
#define BR_SIGNATURE 0xAA55

#define SEC_PER_HEAD 63
#define HEAD_PER_CYL 255
#define HEAD_SIZE (SEC_PER_HEAD * SECTOR_SIZE)
#define CYL_SIZE (SEC_PER_HEAD * HEAD_PER_CYL * SECTOR_SIZE)

#define sec4size(s) ((((s) % CYL_SIZE) % HEAD_SIZE) / SECTOR_SIZE)
#define head4size(s) (((s) % CYL_SIZE) / HEAD_SIZE)
#define cyl4size(s) ((s) / CYL_SIZE)

#define BV_PAGE(bv) ((bv).bv_page)
#define BV_OFFSET(bv) ((bv).bv_offset)
#define BV_LEN(bv) ((bv).bv_len)

#define PART_ENTRY_CSH(__name, __c, __s, __h) \
        .__name##_head   = __h, \
        .__name##_sec    = __s, \
        .__name##_cyl_hi = ((__c) & 0x300), \
        .__name##_cyl    = ((__c) & 0xFF)

#define PART_ENTRY_CSH_BY_LBA(__name, __lba) \
        PART_ENTRY_CSH(__name, (__lba) / (256 * 63), ((__lba) % 63) + 1, ((__lba) / 63) % 256)

#define PART_ENTRY_COORDS_BY_LBA(__start, __length) \
        PART_ENTRY_CSH_BY_LBA(start, __start), \
        PART_ENTRY_CSH_BY_LBA(end, (__start) + (__length) - 1), \
        .abs_start_sec = __start, \
        .sec_in_part = __length

#define IOCTL_BASE 'W'
#define GET_HDDGEO _IOR(IOCTL_BASE, 1, struct hd_geometry)


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eridan Domoratskiy & Eugene Lazurin");
MODULE_DESCRIPTION("Lab 2 I/O systems");


struct part_entry {

    /* 0x00 - Inactive; 0x80 - Active (Bootable) */
    u8  boot_type;

    /* CHS of part first sector, sectors enumerated from one */
    u8  start_head;
    u8  start_sec:6;
    u8  start_cyl_hi:2;
    u8  start_cyl;

    /* 0x83 - primary part, 0x05 - extended part */
    u8  part_type;

    /* CHS of part last sector, sectors enumerated from one */
    u8  end_head;
    u8  end_sec:6;
    u8  end_cyl_hi:2;
    u8  end_cyl;

    /* LBA of part first sector */
    /* https://ru.wikipedia.org/wiki/LBA */
    u32 abs_start_sec;

    /* cound of sectors in part */
    u32 sec_in_part;
} __attribute__ ((__packed__));

typedef struct part_entry part_table[4];

/* Variable for Major Number */
static int c = 0;

/* Disk structure
 *
 * All values in count of sectors.
 *
 * +-----------------------+-----------------------+---------+--------+
 * |    Top-level content  |         Content       |  Start  | Length |
 * +-----------------------+-----------------------+---------+--------+
 * |          MBR          |          MBR          | 0       | 1      |
 * +-----------------------+-----------------------+---------+--------+
 * | Privary volume 10 MiB | Privary volume 10 MiB | 0x800   | 0x5000 |
 * +-----------------------+-----------------------+---------+--------+
 * |                       |          EBR 1        | 0x5800  | 1      |
 * |                       +-----------------------+---------+--------+
 * |    Extended volume    | Logical volume 20 MiB | 0x6000  | 0xA000 |
 * |         40 MiB        +-----------------------+---------+--------+
 * |                       |          EBR 2        | 0x10000 | 1      |
 * |                       +-----------------------+---------+--------+
 * |                       | Logical volume 20 MiB | 0x10800 | 0xA000 |
 * +-----------------------+-----------------------+---------+--------+
 */

static part_table def_part_table = {
    {
        /* not primary */
        .boot_type = 0,

        /* primary part */
        .part_type = 0x83,

        /* coords */
        PART_ENTRY_COORDS_BY_LBA(0x800, 0x5000)
    },
    {
        /* not primary */
        .boot_type = 0,

        /* extended part */
        .part_type = 0x05,

        /* coords */
        PART_ENTRY_COORDS_BY_LBA(0x5800, 0x15800)
    }
};

static unsigned int def_log_part_br_abs_start_sector[] = { 0x5800, 0x10000 };

static const part_table def_log_part_table[] = {
    {
        {
            .boot_type = 0,
            .part_type = 0x83,

            /* coords */
            PART_ENTRY_COORDS_BY_LBA(0x800, 0xA000)
        },
        {
            .boot_type = 0,
            .part_type = 0x05,

            /* coords */
            PART_ENTRY_COORDS_BY_LBA(0xA800, 0x1800)
        }
    },
    {
        {
            .boot_type = 0,
            .part_type = 0x83,

            /* coords */
            PART_ENTRY_COORDS_BY_LBA(0x800, 0xA000)
        }
    }
};

/* Structure associated with Block device */
static struct {
    int size;
    u8 * data;
    spinlock_t lock;
    struct request_queue * queue;
    struct gendisk * gd;
} device;

static void copy_mbr(u8 * disk) {
    memset(disk, 0, MBR_SIZE);

    *(unsigned long *)(disk + MBR_DISK_SIGNATURE_OFFSET) = 0x36E5756D;

    memcpy(disk + PARTITION_TABLE_OFFSET, &def_part_table, PARTITION_TABLE_SIZE);

    *(unsigned short *)(disk + MBR_SIGNATURE_OFFSET) = MBR_SIGNATURE;
}

static void copy_br(u8 * disk, int abs_start_sector, const part_table * part_table) {
    disk += abs_start_sector * SECTOR_SIZE;

    memset(disk, 0, BR_SIZE);
    memcpy(disk + PARTITION_TABLE_OFFSET, part_table, PARTITION_TABLE_SIZE);
    *(unsigned short *)(disk + BR_SIGNATURE_OFFSET) = BR_SIGNATURE;
}

void copy_mbr_n_br(u8 * disk) {
    int i;

    copy_mbr(disk);
    for (i = 0; i < ARRAY_SIZE(def_log_part_table); ++i) {
        copy_br(disk, def_log_part_br_abs_start_sector[i], &def_log_part_table[i]);
    }
}

static int my_open(struct block_device * bdev, fmode_t mode) {
    int ret = 0;

    printk(KERN_INFO "mydiskdrive : open \n");
    return ret;
}

static void my_release(struct gendisk * disk, fmode_t mode) {
    printk(KERN_INFO "mydiskdrive : closed \n");
}

static int my_ioctl(struct block_device * dev, fmode_t mode, unsigned int cmd, unsigned long arg) {
    struct hd_geometry geo;

    switch (cmd) {
        case GET_HDDGEO:
            geo.cylinders = 7;
            geo.heads = 24;
            geo.sectors = 23;
            geo.start = 1;

            if (_copy_to_user((void __user *) arg, &geo, sizeof(geo))) {
                return -EFAULT;
            }

            printk(KERN_INFO "%ld\n", geo.start);
            return 0;
    }

    return -ENOTTY;
}

static struct block_device_operations fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_release,
    .ioctl = my_ioctl,
};

int mydisk_init(void) {
    device.data = vmalloc(MEMSIZE * SECTOR_SIZE);

    /* Setup its partition table */
    copy_mbr_n_br(device.data);

    return MEMSIZE;
}

static int rb_transfer(struct request * req) {
    int dir = rq_data_dir(req);
    int ret = 0;

    /* starting sector where to do operation */
    sector_t start_sector = blk_rq_pos(req);

    /* no of sector on which opn to be done */
    unsigned int sector_cnt = blk_rq_sectors(req);
    struct bio_vec bv;

    struct req_iterator iter;
    sector_t sector_offset = 0;
    unsigned int sectors;
    u8 * buffer;

    rq_for_each_segment(bv, req, iter) {
        buffer = page_address(BV_PAGE(bv)) + BV_OFFSET(bv);

        if (BV_LEN(bv) % (SECTOR_SIZE) != 0) {
            printk(KERN_ERR "bio size is not a multiple ofsector size\n");
            ret = -EIO;
        }

        sectors = BV_LEN(bv) / SECTOR_SIZE;
        printk(
                KERN_DEBUG
                "lab2: Start Sector: %llu, Sector Offset: %llu; "
                "Buffer: %p; Length: %u sectors\n",
                (unsigned long long) start_sector,
                (unsigned long long) sector_offset,
                buffer,
                sectors
        );

        if (dir == WRITE) {
            /* Write to the device */
            memcpy(
                    device.data + (start_sector + sector_offset) * SECTOR_SIZE,
                    buffer,
                    sectors * SECTOR_SIZE
            );
        } else {
            /* Read from the device */
            memcpy(
                    buffer,
                    device.data + (start_sector + sector_offset) * SECTOR_SIZE,
                    sectors * SECTOR_SIZE
            );
        }

        sector_offset += sectors;
    }

    if (sector_offset != sector_cnt) {
        printk(KERN_ERR "mydisk: bio info doesn't match with the request info");
        ret = -EIO;
    }

    return ret;
}

/* request handling function */
static void __device_setup_dev_request(struct request_queue * q) {
    struct request * req;
    int error;

    /* check active request for data transfer */
    while ((req = blk_fetch_request(q)) != NULL) {

        /* transfer the request for operation */
        error = rb_transfer(req);

        /* end the request */
        __blk_end_request_all(req, error);
    }
}

void device_setup(void) {

    /* major no. allocation */
    c = register_blkdev(c, "lab2");
    printk(KERN_ALERT "Major Number is %d", c);

    /* lock for queue */
    spin_lock_init(&device.lock);
    device.queue = blk_init_queue(__device_setup_dev_request, &device.lock);

    /* gendisk allocation */
    device.gd = alloc_disk(8);

    /* major no to gendisk */
    device.gd->major = c;

    /* first minor of gendisk */
    device.gd->first_minor = 0;

    device.gd->fops = &fops;
    device.gd->private_data = &device;
    device.gd->queue = device.queue;
    device.size = mydisk_init();

    printk(KERN_INFO "THIS IS DEVICE SIZE: %d", device.size);

    sprintf(device.gd->disk_name, "lab2");
    set_capacity(device.gd, device.size);
    add_disk(device.gd);
}

static int __init mydiskdrive_init(void) {
    int ret = 0;
    device_setup();

    return ret;
}

void mydisk_cleanup(void) {
    vfree(device.data);
}

void __exit mydiskdrive_exit(void) {
    del_gendisk(device.gd);
    put_disk(device.gd);
    blk_cleanup_queue(device.queue);
    unregister_blkdev(c, "lab2");
    mydisk_cleanup();
}

module_init(mydiskdrive_init);
module_exit(mydiskdrive_exit);
