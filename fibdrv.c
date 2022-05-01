#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 100

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

typedef struct _BigN {
    unsigned int *val;
    unsigned int size;
    int sign;
} BigN;

static BigN *BigN_new(unsigned int size)
{
    BigN *new = kmalloc(sizeof(BigN), GFP_KERNEL);
    new->size = size;
    new->sign = 0;
    new->val = kmalloc(sizeof(unsigned int) * size, GFP_KERNEL);
    memset(new->val, 0, sizeof(unsigned int) * size);

    return new;
}

static void BigN_to_string(char *s, BigN *num)
{
    // log10(x) = log2(x) / log2(10) ~= log2(x) / 3.322
    size_t len = (8 * sizeof(int) * num->size) / 3 + 2 + num->sign;
    char *p = s;

    memset(s, '0', len - 1);
    s[len - 1] = '\0';

    for (int i = num->size - 1; i >= 0; i--) {
        for (unsigned int d = 1U << 31; d; d >>= 1) {
            int carry = ((d & num->val[i]) > 0);
            for (int j = len - 2; j >= 0; j--) {
                s[j] += s[j] - '0' + carry;
                carry = (s[j] > '9');
                if (carry)
                    s[j] -= 10;
            }
        }
    }
    while (p[0] == '0' && p[1] != '\0') {
        p++;
    }
    if (num->sign)
        *(--p) = '-';
    memmove(s, p, strlen(p) + 1);
}

static BigN *BigN_add(const BigN *a, const BigN *b)
{
    unsigned int size = ((a->size > b->size) ? a->size : b->size) + 1;
    BigN *sum = BigN_new(size);

    unsigned int carry = 0;
    unsigned long s = 0;
    for (int i = 0; i < sum->size; i++) {
        unsigned int tmp1 = (a->size) > i ? a->val[i] : 0;
        unsigned int tmp2 = (b->size) > i ? b->val[i] : 0;
        s = (unsigned long) tmp1 + tmp2 + carry;

        sum->val[i] = s & UINT_MAX;
        carry = 0;
        if (s > UINT_MAX) {
            carry = 1;
        }
    }
    if (sum->val[sum->size - 1] == 0) {
        sum->size -= 1;
    }

    return sum;
}


static void BigN_free(BigN *num)
{
    kfree(num->val);
    kfree(num);
}

static BigN *fib_sequence_BigN(long long k)
{
    BigN *a = BigN_new(1);  // f(0) = 0
    BigN *b = BigN_new(1);  // f(1) = 1
    b->val[0] = 1;
    BigN *sum;

    if (k == 0)
        return a;

    for (int i = 2; i <= k; i++) {
        sum = BigN_add(a, b);  // f[i] = f[i - 1] + f[i - 2]
        BigN_free(a);
        a = b;
        b = sum;
    }
    BigN_free(a);

    return b;
}

static long long fib_sequence(long long k)
{
    if (k == 0)
        return 0;
    long long a, b;
    a = 0;
    b = 1;

    for (int i = 2; i <= k; i++) {
        long long c = a + b;
        a = b;
        b = c;
    }
    return b;
}

static long long fast_doubling(long long n)
{
    long long a = 0, b = 1;

    for (unsigned int i = 1U << 31; i; i >>= 1) {
        long long t1 = a * (2 * b - a);  // 2k
        long long t2 = a * a + b * b;    // 2k+1
        if (n & i) {
            a = t2;
            b = t1 + t2;
        } else {
            a = t1;
            b = t2;
        }
    }
    return a;
}

static long long fast_doubling_clz(long long n)
{
    long long a = 0, b = 1;

    for (unsigned int i = 1U << (31 - __builtin_clz(n)); i; i >>= 1) {
        long long t1 = a * (2 * b - a);  // 2k
        long long t2 = a * a + b * b;    // 2k+1
        if (n & i) {
            a = t2;
            b = t1 + t2;
        } else {
            a = t1;
            b = t2;
        }
    }
    return a;
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    char fib[64];

    BigN *n = fib_sequence_BigN(*offset);
    BigN_to_string(fib, n);
    copy_to_user(buf, fib, 64);
    return (ssize_t) fib_sequence(*offset);
}

__attribute__((always_inline)) static inline void escape(void *p)
{
    __asm__ volatile("" : : "g"(p) : "memory");
}

static ktime_t kt;
/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    long long res = 0;
    if (size == 0) {
        kt = ktime_get();
        res = fib_sequence(*offset);
        kt = ktime_sub(ktime_get(), kt);
    } else if (size == 1) {
        kt = ktime_get();
        res = fast_doubling(*offset);
        kt = ktime_sub(ktime_get(), kt);
    } else if (size == 2) {
        kt = ktime_get();
        res = fast_doubling_clz(*offset);
        kt = ktime_sub(ktime_get(), kt);
    } else if (size == 3) {
        kt = ktime_get();
        res = fib_sequence_BigN(*offset);
        kt = ktime_sub(ktime_get(), kt);
    }
    escape(&res);
    return ktime_to_ns(kt);
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
