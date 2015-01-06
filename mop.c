#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Allow mops of capacity (1<<SIZELOG2).
 */
#define SIZELOG2 (31)

/* Show a usage message on stderr for program named av0.
 */
static void showUsage(const char *av0)
{
    static const char usage[] = {
        "%s: Run the %s test over inputs of size <n>.\n"
        "Usage: %s <n> ...\n"
        "Where: <n> ... are integers to a maximum of %lld.\n"
        "Example: %s 0 1 2 9 90 99 999 9999 99999 999990 999999\n"
        "%s: The program aborts when a test fails.\n"
    };
    long long max = 1; max <<= SIZELOG2; max -= 1;
    fprintf(stderr, usage, av0, av0, av0, max, av0, av0);
}


/* For Yarn y, y.root and y.tree are 0 if this is the leaf of a yarn.
   Otherwise y.root is y.tree if this is the root of a yarn.
   Otherwise y.root and y.tree are internal binary trees.
   Add more members to the end if you want.
*/
typedef struct Yarn {
    struct Yarn *root;
    struct Yarn *tree;
} Yarn;

/* m.y[k] for k = 0 to m.knot is either 0 or a yarn of knot k in m.
 */
typedef struct Mop {
    int knot;
    Yarn *y[SIZELOG2];
} Mop;


/* Return a new yarn y of knot 0 and size.
   Call yarnDelete(y) eventually.
*/
static Yarn *yarnNew(size_t size)
{
    Yarn *const result = calloc(1, size);
    result->root = result->tree;
    return result;
}

/* Return yarn y of knot k+1 from 2 left and right yarns of knot k.
   Delete left and right so don't use them after calling this.
   Call yarnDelete(y) eventually.
*/
static Yarn *yarnWeave(Yarn *left, Yarn *right)
{
    assert(left->root == left->tree);
    assert(right->root == right->tree);
    right->root = left->tree;
    left->tree = right;
    left->root = left->tree;
    return left;
}

/* Return an new empty mop m.  Call mopDelete(m) eventually.
 */
static Mop *mopNew(void)
{
    Mop *const result = calloc(1, sizeof *result);
    return result;
}


/* An argument b to mopWring(b, m) and yarnWring(b, y) such that they call
   (*b->yarnF)(b, y) on all yarns y in m, and then call (*b->mopF)(b, m)
   on m.  Add more members to the end if you want.
*/
typedef struct Bucket {
    void (*mopF)(struct Bucket *b, Mop *m);
    void (*yarnF)(struct Bucket *b, Yarn *y);
} Bucket;

/* A null Bucket that does nothing with its mop.
 */
static void nullBucketMop(Bucket *b, Mop *m) {}
static void nullBucketYarn(Bucket *b, Yarn *y) {}
static const Bucket nullBucket = { &nullBucketMop, &nullBucketYarn };

/* Call (*b->yarnF)(b, y) on all binary subtrees of y.
 */
static void yarnWring(Yarn *y, Bucket *b)
{
    assert(y && b);
    if (y->tree) {
        yarnWring(y->tree, b);
        if (y->root != y->tree) {
            yarnWring(y->root, b);
        }
    }
    (*b->yarnF)(b, y);
}

/* Call (*b->mopF)(b, m) after wringing all yarns in m.
 */
static void mopWring(Mop *m, Bucket *b)
{
    assert(m && b);
    const int knot = m->knot;
    for (int k = 0; k <= knot; ++k) {
        Yarn *const y = m->y[k];
        if (y) yarnWring(y, b);
    }
    (*b->mopF)(b, m);
}


/* Delete the mop at m.  Do not use m after calling this.
 */
static void mopFreeMop(Bucket *b, Mop *m) { free(m); }
static void mopFreeYarn(Bucket *b, Yarn *y) { free(y); }
static void mopDelete(Mop *m)
{
    static Bucket bitBucket = { &mopFreeMop, &mopFreeYarn };
    mopWring(m, &bitBucket);
}


/* Return a new mop m containing the items in left and right mops.
   Delete left and right so do not use them after calling this.
   Call mopDelete(m) eventually.
*/
static Mop *mopWeave(Mop *left, Mop *right)
{
    Mop *const result = mopNew();
    const int knot = left->knot > right->knot ? left->knot : right->knot;
    assert(knot < SIZELOG2);
    Yarn *carry = 0;
    for (int k = 0; k <= knot; ++k) {
        const int kind
            = (left->y[k]  ? 1 : 0)
            + (carry       ? 2 : 0)
            + (right->y[k] ? 4 : 0);
        switch (kind) {
        case 0: break;
        case 1:
            result->y[k] = left->y[k];
            break;
        case 2:
            result->y[k] = carry;
            carry = 0;
            break;
        case 3:
            result->y[k] = 0;
            carry = yarnWeave(left->y[k], carry);
            break;
        case 4: result->y[k] = right->y[k]; break;
        case 5:
            result->y[k] = 0;
            carry = yarnWeave(left->y[k], right->y[k]);
            break;
        case 6:
            result->y[k] = 0;
            carry = yarnWeave(carry, right->y[k]);
            break;
        case 7:
            result->y[k] = carry;
            carry = yarnWeave(left->y[k], right->y[k]);
            break;
        default:
            assert(kind < 8);
        }
        left->y[k] = right->y[k] = 0;
    }
    if (carry) {
        result->knot = knot + 1;
        assert(result->knot < SIZELOG2);
        result->y[result->knot] = carry;
    } else {
        result->knot = knot;
    }
    left->knot = right->knot = 0;
    mopDelete(left);
    mopDelete(right);
    return result;
}


/* Return a new mop containing m and y of knot 0.
   m must be returned from mopNew().
   y must be returned from yarnNew().
   Delete m and y so don't use them after calling this.
*/
static Mop *mopAbsorb(Mop *m, Yarn *y)
{
    Mop *const d = mopNew();
    d->y[0] = y;
    return mopWeave(m, d);
}

/* Return a new mop m composed of merging all count mops in mops[].
   Delete mops[n] for all n so don't use them after calling this.
   Call mopDelete(m) eventually.
*/
static Mop *mopRedo(Mop *mops[], long long count)
{
    if (count == 0) return mopNew();
    if (count == 1) return mops[0];
    if (count == 2) return mopWeave(mops[0], mops[1]);
    if (count % 2) return mopWeave(mops[0], mopRedo(mops + 1, count - 1));
    const long long half = count / 2;
    Mop *const left = mopRedo(mops, half);
    Mop *const right = mopRedo(mops + half, half);
    return mopWeave(left, right);
}


/* A yarn of integers containing n for testing.
 */
typedef struct IntYarn {
    struct Yarn *root;
    struct Yarn *tree;
    long long n;
} IntYarn;

typedef struct IntBucket {
    void (*mopF)(Bucket *b, Mop *m);
    void (*yarnF)(Bucket *b, Yarn *y);
    long long *out;
} IntBucket;

static void intYarnF(Bucket *b, Yarn *y)
{
    IntBucket *const iw = (IntBucket *)b;
    IntYarn *const ip = (IntYarn *)y;
    iw->out[ip->n] = ip->n;
}

/* Return a new mop c containing n and the items from m.
   Delete m so do not use m after calling this.
   Call mopDelete(c) eventually.
*/
static Mop *intMopAbsorb(Mop *m, long long n)
{
    IntYarn *const y = (IntYarn *)yarnNew(sizeof *y);
    y->n = n;
    return mopAbsorb(m, (Yarn *)y);
}

/* Return a new mop m containing n.
   Call mopDelete(m) eventually.
*/
static Mop *intMopNew(long long n)
{
    Mop *result = mopNew();
    result = intMopAbsorb(result, n);
    return result;
}

/* Return a new mop m containing all size integers in nv.
   Call mopDelete(m) eventually.
*/
static Mop *intMopRedo(long long size, const long long nv[])
{
    Mop **pMop = calloc(size, sizeof *pMop);
    for (long long i = 0; i < size; ++i) pMop[i] = intMopNew(nv[i]);
    Mop *result = mopRedo(pMop, size);
    free(pMop);
    return result;
}

/* Return a new array v of integers [begin, end).
   Call free(v) eventually.
*/
static long long *range(long long begin, long long end)
{
    const long long size = end - begin;
    long long *result = calloc(size, sizeof *result);
    for (long long i = 0; i < size; ++i) result[i] = begin + i;
    return result;
}

int main(int ac, char *av[])
{
    int ok = 0;
    for (int i = 1; i < ac; ++i) {
        const int begin = 0;
        char *pEnd = av[i];
        const int end = strtoll(av[i], &pEnd, 10);
        if (*pEnd == *"") {
            const long long size = end - begin;
            long long *const in = range(begin, end);
            Mop *const m = intMopRedo(size, in);
            long long *const out = calloc(size, sizeof *out);
            IntBucket b = { nullBucket.mopF, &intYarnF, out };
            mopWring(m, (Bucket *)&b);
            mopDelete(m);
            for (long long i = 0; i < size; ++i) assert(in[i] == out[i]);
            free(out);
            free(in);
            ++ok;
        }
    }
    if (ac > 1 && ac == ok + 1) return 0;
    showUsage(av[0]);
    return 1;
}
