#ifndef _PROM_H_
#define _PROM_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROM_REGISTRY_MAX_COLLECTORS 128
#define PROM_MAX_LABELS 8
#define PROM_MAX_CARDINALITY 8
#define PROM_MAX_LABEL_VALUES_LENGTH 32

#define PROM_TYPE_COUNTER "counter"
#define PROM_TYPE_GAUGE   "gauge"

typedef struct {
    const char *name_space;
    const char *subsystem;
    const char *name;
    const char *help;
} prom_strings_t;


typedef struct {
    double value;
    uint64_t timestamp;
    char label_values[PROM_MAX_LABEL_VALUES_LENGTH];
} prom_metric_t;


typedef size_t(*prom_num_metrics_fn)(void *ctx);
typedef void(*prom_collect_fn)(void *ctx, prom_metric_t *metrics, size_t num_metrics);

typedef struct {
    prom_strings_t *strings;
    size_t num_labels;
    const char **labels;
    void *ctx;
    const char *type;
    prom_num_metrics_fn num_metrics;
    prom_collect_fn collect;
} prom_collector_t;


typedef struct {
    prom_strings_t _strings;
    double _values[PROM_MAX_CARDINALITY];
    uint64_t _timestamps[PROM_MAX_CARDINALITY];
    const char *_labels[PROM_MAX_LABELS];
    size_t _num_labels;
    char _label_values[PROM_MAX_CARDINALITY][PROM_MAX_LABEL_VALUES_LENGTH];
} _prom_base_collector_t;


typedef _prom_base_collector_t prom_counter_t;

void prom_counter_init(prom_counter_t *counter, prom_strings_t args);
void prom_counter_init_vec(prom_counter_t *counter, prom_strings_t args,
    const char **labels, size_t num_labels);
void prom_counter_inc_v(prom_counter_t *counter, double v, ...);

#define prom_counter_inc(counter, ...) prom_counter_inc_v(counter, 1.0, ##__VA_ARGS__)


typedef _prom_base_collector_t prom_gauge_t;

void prom_gauge_init(prom_gauge_t *gauge, prom_strings_t args);
void prom_gauge_init_vec(prom_gauge_t *gauge, prom_strings_t args,
    const char **labels, size_t num_labels);
void prom_gauge_set(prom_gauge_t *gauge, double v, ...);
void prom_gauge_inc_v(prom_gauge_t *gauge, double v, ...);

#define prom_gauge_dec_v(gauge, v, ...) prom_gauge_inc_v(gauge, -(v), ##__VA_ARGS__)
#define prom_gauge_inc(gauge, ...) prom_gauge_inc_v(gauge, 1.0, ##__VA_ARGS__)
#define prom_gauge_dec(gauge, ...) prom_gauge_inc_v(gauge, -1.0, ##__VA_ARGS__)


typedef _prom_base_collector_t prom_summary_t;


typedef _prom_base_collector_t prom_histogram_t;


typedef struct {
    prom_collector_t _collectors[PROM_REGISTRY_MAX_COLLECTORS];
} prom_registry_t;

prom_registry_t *prom_default_registry();
void prom_register(prom_registry_t *registry, prom_collector_t collector);
void prom_register_counter(prom_registry_t *registry, prom_counter_t *counter);
void prom_register_gauge(prom_registry_t *registry, prom_gauge_t *gauge);
void prom_registry_export(prom_registry_t *registry, FILE *w);

uint64_t prom_timestamp();

#ifdef __cplusplus
}
#endif

#endif
