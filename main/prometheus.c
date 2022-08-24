#include "prometheus.h"
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>


prom_registry_t *prom_default_registry() {
    static prom_registry_t reg;
    static bool init = true;
    if (init) {
        memset(&reg, 0, sizeof(prom_registry_t));
        init = false;
    }
    return &reg;
}

uint64_t prom_timestamp() {
    time_t t = time(NULL);
    if (t < 3600*24*365) {
        return 0;
    }
    return (uint64_t)t * 1000;
}


void base_collector_init(_prom_base_collector_t *col, prom_strings_t strings,
    const char **labels, size_t num_labels) {
    assert(num_labels <= PROM_MAX_LABELS);
    memset(&col->_values, 0, sizeof(col->_values));
    memset((void*)&col->_timestamps, 0, sizeof(col->_timestamps));
    memcpy(col->_labels, labels, num_labels*sizeof(char*));
    col->_num_labels = num_labels;
    memset((void*)&col->_label_values, 0, sizeof(col->_label_values));
    col->_strings = strings;
}

size_t base_collector_get_slot(_prom_base_collector_t *col, const char **label_values) {
    if (!col->_num_labels) {
        return 0;
    }

    char joined[PROM_MAX_LABEL_VALUES_LENGTH] = { 0 };
    char *join_ptr = &joined[0];
    for (size_t i = 0; i < col->_num_labels; i++) {
        const char *l = label_values[i];
        assert(l);
        strncpy(join_ptr, l, (&joined[0]+sizeof(joined)) - join_ptr);
        join_ptr += strlen(l);
        *join_ptr = 0;
        join_ptr += 1;
    }

    int slot = -1;
    for (size_t i = 0; i < PROM_MAX_CARDINALITY; i++) {
        if (!col->_label_values[i][0]) {
            memcpy(col->_label_values[i], joined, PROM_MAX_LABEL_VALUES_LENGTH);
            slot = i;
            break;
        }
        if (!memcmp((const char*)&joined, col->_label_values[i], PROM_MAX_LABEL_VALUES_LENGTH)) {
            slot = i;
            break;
        }
    }
    assert(slot >= 0);
    return slot;
}

size_t base_collector_num_metrics(void *ctx) {
    _prom_base_collector_t *col = (_prom_base_collector_t*)ctx;
    if (!col->_num_labels) {
        return 1;
    }
    for (size_t i = 0; i < PROM_MAX_CARDINALITY; i++) {
        if (!col->_label_values[i][0]) {
            return i;
        }
    }
    return 0;
}

void base_collector_collect(void *ctx, prom_metric_t *metrics, size_t num_metrics) {
    _prom_base_collector_t *col = (_prom_base_collector_t*)ctx;
    if (!col->_num_labels) {
        metrics[0].value = col->_values[0];
        metrics[0].timestamp = col->_timestamps[0];
        memset(&metrics[0].label_values, 0, PROM_MAX_LABEL_VALUES_LENGTH);
        return;
    }

    for (size_t j = 0; j < PROM_MAX_CARDINALITY; j++) {
        if (!col->_label_values[j][0]) {
            break;
        }
        metrics[j].value = col->_values[j];
        metrics[j].timestamp = col->_timestamps[j];
        memcpy(metrics[j].label_values, col->_label_values[j], PROM_MAX_LABEL_VALUES_LENGTH);
    }
}


void prom_counter_init(prom_counter_t *counter, prom_strings_t strings) {
    prom_counter_init_vec(counter, strings, NULL, 0);
}

void prom_counter_init_vec(prom_counter_t *counter, prom_strings_t strings,
    const char **labels, size_t num_labels) {
    base_collector_init(counter, strings, labels, num_labels);
}

void prom_counter_set(prom_counter_t *counter, double v, ...) {
    const char *label_values[PROM_MAX_LABELS] = { 0 };
    va_list argp;
    va_start(argp, v);
    for (size_t i = 0; i < counter->_num_labels; i++) {
        const char *l = va_arg(argp, const char*);
        assert(l);
        label_values[i] = l;
    }
    va_end(argp);

    size_t slot = base_collector_get_slot(counter, label_values);
    counter->_values[slot] = v;
    counter->_timestamps[slot] = prom_timestamp();
}

void prom_counter_inc_v(prom_counter_t *counter, double v, ...) {
    const char *label_values[PROM_MAX_LABELS] = { 0 };
    va_list argp;
    va_start(argp, v);
    for (size_t i = 0; i < counter->_num_labels; i++) {
        const char *l = va_arg(argp, const char*);
        assert(l);
        label_values[i] = l;
    }
    va_end(argp);

    assert(v >= 0.0);
    size_t slot = base_collector_get_slot(counter, label_values);
    counter->_values[slot] += v;
    counter->_timestamps[slot] = prom_timestamp();
}


void prom_gauge_init(prom_gauge_t *gauge, prom_strings_t strings) {
    gauge->_strings = strings;
    prom_gauge_init_vec(gauge, strings, NULL, 0);
}

void prom_gauge_init_vec(prom_gauge_t *gauge, prom_strings_t strings,
    const char **labels, size_t num_labels) {
    base_collector_init(gauge, strings, labels, num_labels);
}

void prom_gauge_inc_v(prom_gauge_t *gauge, double v, ...) {
    const char *label_values[PROM_MAX_LABELS] = { 0 };
    va_list argp;
    va_start(argp, v);
    for (size_t i = 0; i < gauge->_num_labels; i++) {
        const char *l = va_arg(argp, const char*);
        assert(l);
        label_values[i] = l;
    }
    va_end(argp);

    size_t slot = base_collector_get_slot(gauge, label_values);
    gauge->_values[slot] += v;
    gauge->_timestamps[slot] = prom_timestamp();
}

void prom_gauge_set(prom_gauge_t *gauge, double v, ...) {
    const char *label_values[PROM_MAX_LABELS] = { 0 };
    va_list argp;
    va_start(argp, v);
    for (size_t i = 0; i < gauge->_num_labels; i++) {
        const char *l = va_arg(argp, const char*);
        assert(l);
        label_values[i] = l;
    }
    va_end(argp);

    size_t slot = base_collector_get_slot(gauge, label_values);
    gauge->_values[slot] = v;
    gauge->_timestamps[slot] = prom_timestamp();
}


void prom_registry_add_global_label(prom_registry_t *registry, const char *label, const char *value) {
    int free_slot = -1;
    for (size_t i = 0; i < PROM_MAX_LABELS; i++) {
        if (!registry->global_labels[i][0]) {
            free_slot = i;
            break;
        }
    }
    assert(free_slot >= 0);
    strncpy(registry->global_labels[free_slot], label, PROM_MAX_LABEL_VALUES_LENGTH);
    strncpy(registry->global_label_values[free_slot], value, PROM_MAX_LABEL_VALUES_LENGTH);
}

void prom_register(prom_registry_t *registry, prom_collector_t collector) {
    int free_slot = -1;
    for (size_t i = 0; i < PROM_REGISTRY_MAX_COLLECTORS; i++) {
        if (!registry->_collectors[i].strings) {
            free_slot = i;
            break;
        }
    }
    assert(free_slot >= 0);
    registry->_collectors[free_slot] = collector;
}

void prom_register_counter(prom_registry_t *registry, prom_counter_t *counter) {
    prom_collector_t col = {
        .strings     = &counter->_strings,
        .num_labels  = counter->_num_labels,
        .labels      = counter->_labels,
        .type        = PROM_TYPE_COUNTER,
        .ctx         = counter,
        .num_metrics = base_collector_num_metrics,
        .collect     = base_collector_collect,
    };
    prom_register(registry, col);
}

void prom_register_gauge(prom_registry_t *registry, prom_gauge_t *gauge) {
    prom_collector_t col = {
        .strings     = &gauge->_strings,
        .num_labels  = gauge->_num_labels,
        .labels      = gauge->_labels,
        .type        = PROM_TYPE_GAUGE,
        .ctx         = gauge,
        .num_metrics = base_collector_num_metrics,
        .collect     = base_collector_collect,
    };
    prom_register(registry, col);
}


void export_strings(FILE *w, prom_strings_t *strings) {
    if (strings->name_space) {
        fprintf(w, "%s_", strings->name_space);
    }
    if (strings->subsystem) {
        fprintf(w, "%s_", strings->subsystem);
    }
    fprintf(w, "%s", strings->name);
}

void export_help(FILE *w, prom_strings_t *strings, const char *type) {
    fprintf(w, "# HELP %s\n", strings->help);
    fprintf(w, "# TYPE ");
    export_strings(w, strings);
    fprintf(w, " %s\n", type);
}

void export_global_labels(FILE *w, prom_registry_t *registry) {
    for (size_t k = 0; k < PROM_MAX_LABELS; k++) {
        const char *label = registry->global_labels[k];
        const char *value = registry->global_label_values[k];
        if (!label[0]) break;
        if (k > 0) fprintf(w, ", ");
        fprintf(w, "%s=\"%s\"", label, value);
    }
}

void export_values(FILE *w, prom_registry_t *registry, prom_collector_t *col) {
    size_t num_metrics = col->num_metrics(col->ctx);
    prom_metric_t metrics[num_metrics];
    col->collect(col->ctx, metrics, num_metrics);

    if (num_metrics == 1 && !metrics[0].label_values[0]) {
        export_strings(w, col->strings);
        fprintf(w, "{");
        export_global_labels(w, registry);
        fprintf(w, "}");
        fprintf(w, " %f", metrics[0].value);
        if (metrics[0].timestamp) {
            fprintf(w, " %llu", metrics[0].timestamp);
        }
        fprintf(w, "\n");
        return;
    }

    for (size_t i = 0; i < num_metrics; i++) {
        prom_metric_t *metric = &metrics[i];
        export_strings(w, col->strings);

        fprintf(w, "{");
        export_global_labels(w, registry);
        bool have_global_labels = !!registry->global_labels[0][0];

        const char *label_val = metric->label_values;
        for (size_t k = 0; k < col->num_labels; k++) {
            if (k > 0 || have_global_labels) fprintf(w, ", ");
            fprintf(w, "%s=\"%s\"", col->labels[k], label_val);
            label_val += strlen(label_val)+1;
        }
        fprintf(w, "} %f", metric->value);

        if (metric->timestamp) {
            fprintf(w, " %llu", metric->timestamp);
        }
        fprintf(w, "\n");
    }
}

void prom_registry_export(prom_registry_t *registry, FILE *w) {
    for (size_t i = 0; i < PROM_REGISTRY_MAX_COLLECTORS; i++) {
        prom_collector_t *col = &registry->_collectors[i];
        if (!col->strings) {
            break;
        }
        export_help(w, col->strings, col->type);
        export_values(w, registry, col);
        fprintf(w, "\n");
    }
}
