/* psort.c - C port of psort Perl script
 * Reads lines from stdin/files, counts occurrences, sorts by count,
 * and outputs with percentages and totals.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

typedef struct {
    char *key;
    long  count;
} Entry;

static Entry *entries    = NULL;
static int    entry_count = 0;
static int    entry_cap   = 0;
static long   tot         = 0;
static int    decimal     = 4;
static int    opt_csv     = 0;
static int    opt_csvall  = 0;
static int    opt_numeric = 0;

static void add_entry(const char *key)
{
    for (int i = 0; i < entry_count; i++) {
        if (strcmp(entries[i].key, key) == 0) {
            entries[i].count++;
            tot++;
            return;
        }
    }
    if (entry_count == entry_cap) {
        entry_cap = entry_cap ? entry_cap * 2 : 64;
        entries = realloc(entries, (size_t)entry_cap * sizeof(Entry));
        if (!entries) { perror("realloc"); exit(1); }
    }
    entries[entry_count].key   = strdup(key);
    entries[entry_count].count = 1;
    if (!entries[entry_count].key) { perror("strdup"); exit(1); }
    entry_count++;
    tot++;
}

static int cmp_by_count(const void *a, const void *b)
{
    const Entry *ea = (const Entry *)a;
    const Entry *eb = (const Entry *)b;
    if (ea->count < eb->count) return -1;
    if (ea->count > eb->count) return  1;
    return 0;
}

static int cmp_by_numeric(const void *a, const void *b)
{
    double va = atof(((const Entry *)a)->key);
    double vb = atof(((const Entry *)b)->key);
    if (va < vb) return -1;
    if (va > vb) return  1;
    return 0;
}

static void pout(const Entry *e, int len)
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
        add_entry(line);
    }
    free(line);
}

static void usage(const char *prog)
{
    printf("Usage: %s [--csv] [--csvall] [--decimal=N] [--numeric] [--help] [file ...]\n"
           "    --csv       : Do output as csv rather than formatted for a human to read\n"
           "    --csvall    : Do output as csv replacing any/all whitespaces with commas\n"
           "    --decimal   : Specify how many decimal places to compute (default 4)\n"
           "    --numeric   : If the data being fed to psort is numeric this flag will sort the\n"
           "                :  output by that data instead of by counts\n"
           "    --help      : Output this help info\n"
           "\n"
           "Synopsis: Takes input and essentially does the equivalent of `sort | uniq -c | sort -n`\n"
           "with added percentages and total info, default output is nicely formatted for humans to read.\n",
           prog);
}

int main(int argc, char *argv[])
{
    static const struct option long_options[] = {
        { "csv",     no_argument,       NULL, 'c' },
        { "csvall",  no_argument,       NULL, 'a' },
        { "decimal", required_argument, NULL, 'd' },
        { "numeric", no_argument,       NULL, 'n' },
        { "help",    no_argument,       NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "cad:nh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c': opt_csv     = 1;            break;
            case 'a': opt_csvall  = 1;            break;
            case 'd': decimal     = atoi(optarg); break;
            case 'n': opt_numeric = 1;            break;
            case 'h': usage(argv[0]); return 0;
            default:  usage(argv[0]); return 1;
        }
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

    qsort(entries, (size_t)entry_count, sizeof(Entry),
          opt_numeric ? cmp_by_numeric : cmp_by_count);

    /* width of the total count for column alignment */
    int len = snprintf(NULL, 0, "%ld", tot);

    for (int i = 0; i < entry_count; i++)
        pout(&entries[i], len);

    if (opt_csv || opt_csvall)
        printf("%8s,%*ld\n", "Total:", len, tot);
    else
        printf("%8s  %*ld\n", "Total:", len, tot);

    for (int i = 0; i < entry_count; i++)
        free(entries[i].key);
    free(entries);

    return 0;
}
