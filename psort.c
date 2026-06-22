/* psort.c - C port of psort Perl script
 * Reads lines from stdin/files, counts occurrences, sorts by count,
 * and outputs with percentages and totals.
 */
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* ── hash table (open addressing, FNV-1a, load ≤ 65 %) ─────────────────── */

typedef struct {
    char *key;    /* NULL = empty slot */
    long  count;
} HSlot;

static HSlot *htable   = NULL;
static int    ht_cap   = 0;   /* always a power of 2 */
static int    ht_used  = 0;

static unsigned fnv1a(const char *s)
{
    unsigned h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

static void ht_grow(void)
{
    int new_cap = ht_cap ? ht_cap * 2 : 4096;
    HSlot *nt = calloc((size_t)new_cap, sizeof(HSlot));
    if (!nt) { perror("calloc"); exit(1); }
    int mask = new_cap - 1;
    for (int i = 0; i < ht_cap; i++) {
        if (!htable[i].key) continue;
        int j = (int)(fnv1a(htable[i].key) & (unsigned)mask);
        while (nt[j].key) j = (j + 1) & mask;
        nt[j] = htable[i];
    }
    free(htable);
    htable = nt;
    ht_cap = new_cap;
}

/* Return the slot for key, inserting a new zero-count slot if absent. */
static HSlot *ht_get(const char *key)
{
    if (ht_used * 100 >= ht_cap * 65)   /* load > 65 % → grow first */
        ht_grow();
    int mask = ht_cap - 1;
    int i = (int)(fnv1a(key) & (unsigned)mask);
    for (;;) {
        if (!htable[i].key) {
            htable[i].key = strdup(key);
            if (!htable[i].key) { perror("strdup"); exit(1); }
            htable[i].count = 0;
            ht_used++;
            return &htable[i];
        }
        if (strcmp(htable[i].key, key) == 0)
            return &htable[i];
        i = (i + 1) & mask;
    }
}

/* ── output entry list (built after all input is read) ─────────────────── */

typedef struct {
    char *key;
    long  count;
} Entry;

static Entry *entries    = NULL;
static int    entry_count = 0;

static long  tot          = 0;
static int   decimal      = 4;
static int   opt_csv      = 0;
static int   opt_csvall   = 0;
static int   opt_numeric  = 0;
static long  opt_min      = 0;
static long  opt_max      = 0;
static int   opt_fullperc = 0;
static int   opt_ignore   = 0;
static int   opt_verbose  = 0;
static int   opt_reverse   = 0;
static int   opt_histogram = 0;
static int   opt_date      = 0;

/* Collect live hash table slots into the entries[] array. */
static void ht_to_entries(void)
{
    entries = malloc((size_t)ht_used * sizeof(Entry));
    if (!entries) { perror("malloc"); exit(1); }
    entry_count = 0;
    for (int i = 0; i < ht_cap; i++) {
        if (!htable[i].key) continue;
        entries[entry_count].key   = htable[i].key;
        entries[entry_count].count = htable[i].count;
        entry_count++;
    }
}

static Entry *new_entry(const char *key)
{
    /* Only called after ht_to_entries(), for the "[excluded]" sentinel. */
    entries = realloc(entries, (size_t)(entry_count + 1) * sizeof(Entry));
    if (!entries) { perror("realloc"); exit(1); }
    entries[entry_count].key   = strdup(key);
    entries[entry_count].count = 0;
    if (!entries[entry_count].key) { perror("strdup"); exit(1); }
    return &entries[entry_count++];
}

static const char *date_fmts[] = {
    "%d %b %Y",
    "%b %d %Y",
    "%m/%d/%Y",
    "%m/%d/%y",
    "%Y-%m-%d",
    "%d-%b-%Y",
    NULL
};

static time_t parse_date(const char *s)
{
    struct tm tm;
    for (int i = 0; date_fmts[i]; i++) {
        memset(&tm, 0, sizeof(tm));
        if (strptime(s, date_fmts[i], &tm))
            return mktime(&tm);
    }
    return (time_t)0;
}

static int cmp_by_date(const void *a, const void *b)
{
    time_t da = parse_date(((const Entry *)a)->key);
    time_t db = parse_date(((const Entry *)b)->key);
    int r = (da > db) - (da < db);
    return opt_reverse ? -r : r;
}

static int cmp_by_count(const void *a, const void *b)
{
    const Entry *ea = (const Entry *)a;
    const Entry *eb = (const Entry *)b;
    int r = (ea->count > eb->count) - (ea->count < eb->count);
    return opt_reverse ? -r : r;
}

static int cmp_by_numeric(const void *a, const void *b)
{
    double va = atof(((const Entry *)a)->key);
    double vb = atof(((const Entry *)b)->key);
    int r = (va > vb) - (va < vb);
    return opt_reverse ? -r : r;
}

static int term_width(void)
{
    const char *s = getenv("COLUMNS");
    if (s) { int c = atoi(s); if (c > 0) return c; }
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return (int)ws.ws_col;
    return 80;
}

static void pout(const Entry *e, int len, int bar_inner, int max_key)
{
    double p = ((double)e->count / (double)tot) * 100.0;
    char *key = e->key;
    char *csvkey = NULL;

    if (opt_csvall) {
        csvkey = strdup(key);
        if (!csvkey) { perror("strdup"); exit(1); }
        for (char *cp = csvkey; *cp; cp++) {
            if (*cp == ' ' || *cp == '\t') *cp = ',';
        }
        key = csvkey;
    }

    if (opt_csv || opt_csvall) {
        if (p < 10.0)
            printf("%1.*f%%,%ld,%s\n",  decimal, p, e->count, key);
        else
            printf("%2.*f%%,%ld,%s\n",  decimal, p, e->count, key);
    } else if (opt_histogram) {
        int filled = (int)(p / 100.0 * bar_inner);
        if (filled > bar_inner) filled = bar_inner;
        char *bar = malloc((size_t)(bar_inner + 3));
        if (!bar) { perror("malloc"); exit(1); }
        bar[0] = '[';
        memset(bar + 1, '#', (size_t)filled);
        memset(bar + 1 + filled, ' ', (size_t)(bar_inner - filled));
        bar[bar_inner + 1] = ']';
        bar[bar_inner + 2] = '\0';
        char perc[32];
        if (p < 10.0)
            snprintf(perc, sizeof(perc), " %1.*f%%", decimal, p);
        else
            snprintf(perc, sizeof(perc), "%2.*f%%", decimal, p);
        printf("%s  %*ld %-*s %s\n", perc, len, e->count, max_key, key, bar);
        free(bar);
    } else {
        if (p < 10.0)
            printf(" %1.*f%%  %*ld %s\n", decimal, p, len, e->count, key);
        else
            printf("%2.*f%%  %*ld %s\n",  decimal, p, len, e->count, key);
    }

    free(csvkey);
}

static void process_stream(FILE *f)
{
    char   *line    = NULL;
    size_t  linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&line, &linecap, f)) > 0) {
        if (linelen > 0 && line[linelen - 1] == '\n')
            line[--linelen] = '\0';
        ht_get(line)->count++;
        tot++;
    }
    free(line);
}

static void usage(const char *prog)
{
    printf("Usage: %s [options] [file ...]\n"
           "    --csv, -c    : Do output as csv rather than formatted for a human to read\n"
           "    --csvall, -a : Do output as csv replacing any/all whitespaces with commas\n"
           "    --decimal, -d: Specify how many decimal places to compute (default 4)\n"
           "    --numeric    : If the data being fed to psort is numeric this flag will sort the\n"
           "                 :  output by that data instead of by counts\n"
           "    --date       : Sort output by date value of data; recognises common formats:\n"
           "                 :  \"22 Jun 2026\", \"Jun 22 2026\", \"06/22/26\", \"06/22/2026\",\n"
           "                 :  \"2026-06-22\", \"22-Jun-2026\"\n"
           "    --min, -m    : Give a minimum count below which we should throw out those, to\n"
           "                    allow trimming smaller values (compute %% based upon what's left)\n"
           "    --max, -x    : Give a maximum count above which we should throw out those, to\n"
           "                    allow trimming larger values  (compute %% based upon what's left)\n"
           "    --fullperc, -f: Only with a min/max, still output the %% based upon the full count\n"
           "                    vs only what matched the filter\n"
           "    --ignore, -i : When using min/max, don't include the [excluded] line\n"
           "                    that shows us the full 100%%\n"
           "    --verbose, -v: When using min/max, print key/count totals before and after filtering\n"
           "    --reverse,-r : Reverse the sort order (highest count/value first)\n"
           "    --histogram,-H: Show a bar histogram scaled to terminal width\n"
           "    --help, -h   : Output this help info\n"
           "\n"
           "Synopsis: Takes input and essentially does the equivalent of `sort | uniq -c | sort -n`\n"
           "with added percentages and total info, default output is nicely formatted for humans to read.\n",
           prog);
}

#define OPT_NUMERIC    256
#define OPT_HISTOGRAM  257
#define OPT_DATE       258

int main(int argc, char *argv[])
{
    static const struct option long_options[] = {
        { "csv",      no_argument,       NULL, 'c' },
        { "csvall",   no_argument,       NULL, 'a' },
        { "decimal",  required_argument, NULL, 'd' },
        { "numeric",  no_argument,       NULL, OPT_NUMERIC },
        { "date",     no_argument,       NULL, OPT_DATE },
        { "min",      required_argument, NULL, 'm' },
        { "max",      required_argument, NULL, 'x' },
        { "fullperc", no_argument,       NULL, 'f' },
        { "ignore",   no_argument,       NULL, 'i' },
        { "verbose",  no_argument,       NULL, 'v' },
        { "reverse",   no_argument,       NULL, 'r' },
        { "histogram", no_argument,       NULL, 'H' },
        { "help",      no_argument,       NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "cad:m:x:fivrHh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c': opt_csv      = 1;            break;
            case 'a': opt_csvall   = 1;            break;
            case 'd': decimal      = atoi(optarg); break;
            case OPT_NUMERIC: opt_numeric = 1;     break;
            case OPT_DATE:    opt_date    = 1;     break;
            case 'm': opt_min      = atol(optarg); break;
            case 'x': opt_max      = atol(optarg); break;
            case 'f': opt_fullperc = 1;            break;
            case 'i': opt_ignore   = 1;            break;
            case 'v': opt_verbose  = 1;            break;
            case 'r': opt_reverse   = 1;            break;
            case 'H': opt_histogram = 1;            break;
            case 'h': usage(argv[0]); return 0;
            default:  usage(argv[0]); return 1;
        }
    }

    if (opt_fullperc && !opt_min && !opt_max) {
        fprintf(stderr, "ERROR: cannot use fullperc without either min or max\n");
        usage(argv[0]);
        return 1;
    }

    if (optind < argc) {
        for (int i = optind; i < argc; i++) {
            FILE *f = fopen(argv[i], "r");
            if (!f) { perror(argv[i]); continue; }
            process_stream(f);
            fclose(f);
        }
    } else {
        process_stream(stdin);
    }

    if (tot == 0) return 0;

    ht_to_entries();   /* move hash table contents into sortable entries[] */

    if (opt_min || opt_max) {
        if (opt_verbose) {
            long tc = 0;
            for (int j = 0; j < entry_count; j++) tc += entries[j].count;
            fprintf(stderr, "# keys/counts before: %d / %ld\n", entry_count, tc);
        }

        long excluded_count = 0;
        int i = 0;
        while (i < entry_count) {
            Entry *e = &entries[i];
            int drop = (opt_min && e->count < opt_min) || (opt_max && e->count > opt_max);
            if (drop) {
                excluded_count += e->count;
                if (!opt_fullperc) tot -= e->count;
                free(e->key);
                *e = entries[--entry_count];
            } else {
                i++;
            }
        }

        if (excluded_count && !opt_ignore) {
            Entry *ex = new_entry("[excluded]");
            ex->count = excluded_count;
        }

        if (opt_verbose) {
            long tc = 0;
            for (int j = 0; j < entry_count; j++) tc += entries[j].count;
            fprintf(stderr, "# keys/counts after: %d / %ld\n", entry_count, tc);
        }
    }

    if (tot == 0) return 0;

    qsort(entries, (size_t)entry_count, sizeof(Entry),
          opt_date ? cmp_by_date : (opt_numeric ? cmp_by_numeric : cmp_by_count));

    int len = snprintf(NULL, 0, "%ld", tot);

    int bar_inner = 0;
    int max_key = 0;
    if (opt_histogram && !opt_csv && !opt_csvall) {
        for (int j = 0; j < entry_count; j++) {
            int kl = (int)strlen(entries[j].key);
            if (kl > max_key) max_key = kl;
        }
        bar_inner = term_width() - decimal - len - max_key - 11;
        if (bar_inner < 10) bar_inner = 10;
    }

    for (int i = 0; i < entry_count; i++)
        pout(&entries[i], len, bar_inner, max_key);

    if (opt_histogram && !opt_csv && !opt_csvall)
        printf("%*s  %*ld\n", decimal + 4, "Total:", len, tot);
    else if (opt_csv || opt_csvall)
        printf("%8s,%*ld\n", "Total:", len, tot);
    else
        printf("%8s  %*ld\n", "Total:", len, tot);

    for (int i = 0; i < entry_count; i++)
        free(entries[i].key);
    free(entries);
    free(htable);

    return 0;
}
