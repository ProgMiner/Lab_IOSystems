#include "lab3.h"

#include <linux/module.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/moduleparam.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <net/arp.h>
#include <net/protocol.h>
#include <linux/icmp.h>
#include <linux/proc_fs.h>


#define PROC_NAME "lab3"

#define INTERFACE_FORMAT_NAME "lab3"
#define PARENT_INTERFACE_NAME "enp0s3"

#define TRUE 1
#define FALSE 0


MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eugene Lazurin & Eridan Domoratskiy");
MODULE_DESCRIPTION("Lab 3 I/O systems");


struct priv {
    struct net_device * parent;
    struct net_device_stats stats;
};

static struct net_device * ndev;
static struct proc_dir_entry * proc;

static struct lab3_history * history;

static int handle_icmp(struct sk_buff * skb) {
    struct iphdr * ip = (struct iphdr *) skb_network_header(skb);

    if (ip->protocol == IPPROTO_ICMP) {
        u16 ip_hdr_len = ip->ihl * 4;

        struct icmphdr * icmp = (struct icmphdr *) ((u8 *) ip + ip_hdr_len);

        if (icmp->type == /* 0 */ ICMP_ECHO) {
            u16 off = ip_hdr_len + sizeof(struct icmphdr);
            u16 data_len = ntohs(ip->tot_len) - off;
            u8 * icmp_data = skb->data + off;

            history = lab3_history_new(icmp_data, data_len, history);
            return TRUE;
        }
    }

    return FALSE;
}

static rx_handler_result_t handle_frame(struct sk_buff ** pskb) {
    struct sk_buff * skb = * pskb;

    struct priv *priv = netdev_priv(ndev);
    printk(KERN_INFO "%s: handled frame.\n", THIS_MODULE->name);

    if (handle_icmp(skb) == TRUE) {
        printk(KERN_INFO "%s: it was my IMCP packet!\n", THIS_MODULE->name);

        priv->stats.rx_packets++;
        priv->stats.rx_bytes += skb->len;
    }

    return RX_HANDLER_PASS;
}

static struct net_device_stats * ndev_stats(struct net_device * dev) {
    struct priv * priv = netdev_priv(dev);
    return &priv->stats;
}

static int ndev_open(struct net_device * dev) {
    netif_start_queue(dev);

    printk(KERN_INFO "%s: device opened: name=%s\n", THIS_MODULE->name, dev->name);
    return 0;
}

static int ndev_stop(struct net_device *dev) {
    netif_stop_queue(dev);

    printk(KERN_INFO "%s: device closed: name=%s\n", THIS_MODULE->name, dev->name);
    return 0;
}

static netdev_tx_t ndev_start_xmit(struct sk_buff *skb, struct net_device *dev) {
    struct priv * priv = netdev_priv(dev);

    priv->stats.tx_packets++;
    priv->stats.tx_bytes += skb->len;

    if (priv->parent) {
        skb->dev = priv->parent;
        skb->priority = 1;
        dev_queue_xmit(skb);
    }

    return NETDEV_TX_OK;
}

static struct net_device_ops nops = {
    .ndo_open = ndev_open,
    .ndo_stop = ndev_stop,
    .ndo_get_stats = ndev_stats,
    .ndo_start_xmit = ndev_start_xmit,
};

static void ndev_setup(struct net_device * dev) {
    ether_setup(dev);

    memset(netdev_priv(dev), 0, sizeof(struct priv));
    dev->netdev_ops = &nops;
}

ssize_t proc_read(struct file * filp, char __user * ubuf, size_t count, loff_t * off) {
    size_t len;
    char * buf;

    if (*off != 0) {
        return 0;
    }

    // TODO lock
    len = lab3_history_print(history, &buf);
    if (count < len) {
        len = count;
    }

    if (len == 0) {
        kfree(buf);
        return 0;
    }

    if (copy_to_user(ubuf, buf, len) != 0) {
        kfree(buf);
        return -EFAULT;
    }

    kfree(buf);
    *off += len;
    return len;
}

static struct file_operations proc_fops = {
    .owner = THIS_MODULE,
    .read = proc_read,
};

int __init vni_init(void) {
    struct priv * priv;
    int err;

    ndev = alloc_netdev(sizeof(struct priv), INTERFACE_FORMAT_NAME,
            NET_NAME_PREDICTABLE, ndev_setup);

    if (ndev == NULL) {
        printk(KERN_ERR "%s: unable to allocate net device\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    priv = netdev_priv(ndev);
    priv->parent = __dev_get_by_name(&init_net, PARENT_INTERFACE_NAME);

    if (!priv->parent) {
        printk(KERN_ERR "%s: unable to find parent for net device: %s\n",
                THIS_MODULE->name, PARENT_INTERFACE_NAME);

        free_netdev(ndev);
        return -ENODEV;
    }

    if (priv->parent->type != ARPHRD_ETHER && priv->parent->type != ARPHRD_LOOPBACK) {
        printk(KERN_ERR "%s: illegal net type\n", THIS_MODULE->name);
        free_netdev(ndev);
        return -EINVAL;
    }

    // copy IP, MAC and other information
    memcpy(ndev->dev_addr, priv->parent->dev_addr, ETH_ALEN);
    memcpy(ndev->broadcast, priv->parent->broadcast, ETH_ALEN);

    err = dev_alloc_name(ndev, ndev->name);
    if (err < 0) {
        printk(KERN_ERR "%s: failed to allocate name, error %i\n", THIS_MODULE->name, err);
        free_netdev(ndev);
        return -EIO;
    }

    if ((proc = proc_create(PROC_NAME, 0444, NULL, &proc_fops)) == NULL) {
        printk(KERN_ERR "%s: failed to create a proc entry: proc=%s\n",
                THIS_MODULE->name, PROC_NAME);

        free_netdev(ndev);
        return -EIO;
    }

    register_netdev(ndev);

    rtnl_lock();
    netdev_rx_handler_register(priv->parent, handle_frame, NULL);
    rtnl_unlock();

    printk(KERN_INFO "Module %s loaded", THIS_MODULE->name);
    printk(KERN_INFO "%s: create link %s", THIS_MODULE->name, ndev->name);
    printk(KERN_INFO "%s: registered rx handler for %s", THIS_MODULE->name, priv->parent->name);
    return 0;
}

void __exit vni_exit(void) {
    struct priv * priv = netdev_priv(ndev);

    if (priv->parent) {
        rtnl_lock();
        netdev_rx_handler_unregister(priv->parent);
        rtnl_unlock();
        printk(KERN_INFO "%s: unregister rx handler for %s",
                THIS_MODULE->name, priv->parent->name);
    }

    unregister_netdev(ndev);
    free_netdev(ndev);
    proc_remove(proc);
    lab3_history_delete(history);

    printk(KERN_INFO "Module %s unloaded", THIS_MODULE->name);
}

module_init(vni_init);
module_exit(vni_exit);
