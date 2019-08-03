#ifndef _PROM_H_
#define _PROM_H_

#include <stddef.h>

#define PROM_REGISTRY_MAX_COLLECTORS 128
#define PROM_MAX_LABELS 8
#define PROM_MAX_CARDINALITY 8
#define PROM_MAX_LABEL_VALUES_LENGTH 32


typedef struct {
    const char *namespace;
    const char *subsystem;
    const char *name;
    const char *help;
} prom_strings_t;

typedef struct {
    prom_strings_t _strings;
    double _count;
} prom_counter_t;

void prom_counter_init(prom_counter_t *counter, prom_strings_t args);
void prom_counter_inc(prom_counter_t *counter);
void prom_counter_inc_v(prom_counter_t *counter, double v);


typedef struct {
    prom_strings_t _strings;

    double _values[PROM_MAX_CARDINALITY];
    const char *_labels[PROM_MAX_LABELS];
    size_t _num_labels;
    char _label_values[PROM_MAX_CARDINALITY][PROM_MAX_LABEL_VALUES_LENGTH];
} prom_gauge_t;

void prom_gauge_init(prom_gauge_t *gauge, prom_strings_t args);
void prom_gauge_init_vec(prom_gauge_t *gauge, prom_strings_t args,
    const char **labels, size_t num_labels);
void prom_gauge_inc(prom_gauge_t *gauge);
void prom_gauge_inc_v(prom_gauge_t *gauge, double v);
void prom_gauge_dec(prom_gauge_t *gauge);
void prom_gauge_dec_v(prom_gauge_t *gauge, double v);
void prom_gauge_set(prom_gauge_t *gauge, double v, ...);


typedef struct {
} prom_summary_t;


typedef struct {
} prom_histogram_t;


typedef enum {
    PROM_COLLECTOR_INVALID = 0,
    PROM_COLLECTOR_COUNTER,
    PROM_COLLECTOR_GAUGE,
    PROM_COLLECTOR_SUMMARY,
    PROM_COLLECTOR_HISTOGRAM,
    PROM_COLLECTOR_MAX,
} collector_type_t;

typedef struct {
    collector_type_t _type;
    void *_col;
} collector_t;

typedef struct {
    collector_t _collectors[PROM_REGISTRY_MAX_COLLECTORS];
} prom_registry_t;

prom_registry_t *prom_default_registry();
void prom_register_counter(prom_registry_t *registry, prom_counter_t *collector);
void prom_register_gauge(prom_registry_t *registry, prom_gauge_t *collector);
int prom_format(prom_registry_t *registry, char *buf, size_t buf_len);


#endif
