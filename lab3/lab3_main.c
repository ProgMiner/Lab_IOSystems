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


MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eugene Lazurin & Eridan Domoratskiy");
MODULE_DESCRIPTION("Lab 3 I/O systems");


static const char * const ifaces[] = {"enp0s3", "eth0", "lo"};
#define ifaces_length (sizeof(ifaces) / sizeof(char *))

struct priv {
    struct net_device * parents[ifaces_length];
    struct net_device_stats stats;
};

static struct net_device * ndev;
static struct proc_dir_entry * proc;

static struct lab3_history * history;

static struct net_device * priv_first_parent(struct priv * priv) {
    unsigned int i = 0;

    for (i = 0; i < ifaces_length; ++i) {
        if (priv->parents[i]) {
            return priv->parents[i];
        }
    }

    return NULL;
}

static bool handle_icmp(struct sk_buff * skb) {
    u16 ip_hdr_len, off, data_len;
    struct icmphdr * icmp;
    u8 * icmp_data;

    struct iphdr * ip = (struct iphdr *) skb_network_header(skb);
    if (ip->protocol == IPPROTO_ICMP) {
        ip_hdr_len = ip->ihl * 4;

        icmp = (struct icmphdr *) ((u8 *) ip + ip_hdr_len);
        if (icmp->type == ICMP_ECHO) {
            off = ip_hdr_len + sizeof(struct icmphdr);
            data_len = ntohs(ip->tot_len) - off;
            icmp_data = skb->data + off;

            history = lab3_history_new(skb->dev->name, icmp_data, data_len, history);
            return true;
        }
    }

    return false;
}

static rx_handler_result_t handle_frame(struct sk_buff ** pskb) {
    struct sk_buff * skb = * pskb;

    struct priv * priv = netdev_priv(ndev);
    printk(KERN_INFO "%s: handled frame.\n", THIS_MODULE->name);

    if (handle_icmp(skb)) {
        printk(KERN_INFO "%s: it was my IMCP packet!\n", THIS_MODULE->name);

        ++priv->stats.rx_packets;
        priv->stats.rx_bytes += skb->len;
    }

    return RX_HANDLER_PASS;
}

static struct net_device_stats * ndev_stats(struct net_device * dev) {
    return &(((struct priv *) netdev_priv(dev))->stats);
}

static int ndev_open(struct net_device * dev) {
    netif_start_queue(dev);

    printk(KERN_INFO "%s: device opened: name=%s\n", THIS_MODULE->name, dev->name);
    return 0;
}

static int ndev_stop(struct net_device * dev) {
    netif_stop_queue(dev);

    printk(KERN_INFO "%s: device closed: name=%s\n", THIS_MODULE->name, dev->name);
    return 0;
}

static netdev_tx_t ndev_start_xmit(struct sk_buff * skb, struct net_device * dev) {
    struct priv * priv = netdev_priv(dev);

    priv->stats.tx_packets++;
    priv->stats.tx_bytes += skb->len;

    skb->dev = priv_first_parent(priv);
    skb->priority = 1;

    if (skb->dev) {
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

static void fill_priv_parents(struct priv * priv) {
    unsigned int i;

    for (i = 0; i < ifaces_length; ++i) {
        priv->parents[i] = __dev_get_by_name(&init_net, ifaces[i]);

        if (!priv->parents[i]) {
            printk(KERN_WARNING "%s: unable to find parent for net device: %s\n",
                    THIS_MODULE->name, ifaces[i]);

            continue;
        }

        if (priv->parents[i]->type != ARPHRD_ETHER && priv->parents[i]->type != ARPHRD_LOOPBACK) {
            printk(KERN_WARNING "%s: illegal net type of device, ignoring it: %s\n",
                    THIS_MODULE->name, ifaces[i]);

            priv->parents[i] = NULL;
        }
    }
}

static void register_rx_handlers(struct priv * priv) {
    unsigned int i;
    int ret;

    for (i = 0; i < ifaces_length; ++i) {
        if (priv->parents[i]) {

            rtnl_lock();
            ret = netdev_rx_handler_register(priv->parents[i], handle_frame, NULL);
            rtnl_unlock();

            if (ret) {
                printk(KERN_WARNING "%s: cannot register rx handler for %s",
                        THIS_MODULE->name, ifaces[i]);

                priv->parents[i] = NULL;
            } else {
                printk(KERN_INFO "%s: registered rx handler for %s",
                        THIS_MODULE->name, ifaces[i]);
            }
        }
    }
}

int __init vni_init(void) {
    struct net_device * first_parent;
    struct priv * priv;
    int err, ret = 0;

    ndev = alloc_netdev(
            sizeof(struct priv),
            INTERFACE_FORMAT_NAME,
            NET_NAME_PREDICTABLE,
            ndev_setup
    );

    if (!ndev) {
        printk(KERN_ERR "%s: unable to allocate net device\n", THIS_MODULE->name);
        ret = -ENOMEM;
        goto out;
    }

    priv = netdev_priv(ndev);
    fill_priv_parents(priv);

    if (!(first_parent = priv_first_parent(priv))) {
        printk(KERN_ERR "%s: unable to find any suitable parent for net device\n",
                THIS_MODULE->name);

        ret = -ENODEV;
        goto free_ndev;
    }

    // copy IP, MAC and other information
    memcpy(ndev->dev_addr, first_parent->dev_addr, ETH_ALEN);
    memcpy(ndev->broadcast, first_parent->broadcast, ETH_ALEN);

    err = dev_alloc_name(ndev, ndev->name);
    if (err < 0) {
        printk(KERN_ERR "%s: failed to allocate name, error %i\n", THIS_MODULE->name, err);

        ret = -EIO;
        goto free_ndev;
    }

    if (!(proc = proc_create(PROC_NAME, 0444, NULL, &proc_fops))) {
        printk(KERN_ERR "%s: failed to create a proc entry: proc=%s\n",
                THIS_MODULE->name, PROC_NAME);

        ret = -EIO;
        goto free_ndev;
    }

    register_rx_handlers(priv);

    if (!priv_first_parent(priv)) {
        printk(KERN_ERR "%s: unable to register any rx handler\n", THIS_MODULE->name);

        ret = -EBUSY;
        goto free_ndev;
    }

    register_netdev(ndev);

    printk(KERN_INFO "Module %s loaded", THIS_MODULE->name);
    printk(KERN_INFO "%s: create link %s", THIS_MODULE->name, ndev->name);
    goto out;

free_ndev:
    free_netdev(ndev);

out:
    return ret;
}

void __exit vni_exit(void) {
    struct priv * priv = netdev_priv(ndev);
    unsigned int i;

    for (i = 0; i < ifaces_length; ++i) {
        if (priv->parents[i]) {

            rtnl_lock();
            netdev_rx_handler_unregister(priv->parents[i]);
            rtnl_unlock();

            printk(KERN_INFO "%s: unregister rx handler for %s",
                    THIS_MODULE->name, ifaces[i]);
        }
    }

    unregister_netdev(ndev);
    free_netdev(ndev);

    proc_remove(proc);
    lab3_history_delete(history);

    printk(KERN_INFO "Module %s unloaded", THIS_MODULE->name);
}

module_init(vni_init);
module_exit(vni_exit);
