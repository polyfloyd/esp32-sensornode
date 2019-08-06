#include "prometheus.h"
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>


prom_registry_t *prom_default_registry() {
    static prom_registry_t reg;
    static bool init = false;
    if (init) {
        memset(&reg, 0, sizeof(prom_registry_t));
        init = false;
    }
    return &reg;
}


void base_collector_init(_prom_base_collector_t *col, prom_strings_t strings,
    const char **labels, size_t num_labels) {
    assert(num_labels <= PROM_MAX_LABELS);
    memset(&col->_values, 0, sizeof(col->_values));
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
        if (!strncmp((const char*)&joined, col->_label_values[i], PROM_MAX_LABEL_VALUES_LENGTH)) {
            slot = i;
            break;
        }
    }
    assert(slot >= 0);
    return slot;
}


void prom_counter_init(prom_counter_t *counter, prom_strings_t strings) {
    prom_counter_init_vec(counter, strings, NULL, 0);
}

void prom_counter_init_vec(prom_counter_t *counter, prom_strings_t strings,
    const char **labels, size_t num_labels) {
    base_collector_init(counter, strings, labels, num_labels);
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
}


void prom_register(prom_registry_t *registry, collector_type_t type, void *collector) {
    int free_slot = -1;
    for (size_t i = 0; i < PROM_REGISTRY_MAX_COLLECTORS; i++) {
        if (!registry->_collectors[i]._type) {
            free_slot = i;
            break;
        }
    }
    assert(free_slot >= 0);

    collector_t *slot = &registry->_collectors[free_slot];
    slot->_type = type;
    slot->_col  = collector;
}

void prom_register_counter(prom_registry_t *registry, prom_counter_t *collector) {
    prom_register(registry, PROM_COLLECTOR_COUNTER, collector);
}

void prom_register_gauge(prom_registry_t *registry, prom_gauge_t *collector) {
    prom_register(registry, PROM_COLLECTOR_GAUGE, collector);
}


void export_strings(FILE *w, prom_strings_t strings) {
    if (strings.namespace) {
        fprintf(w, "%s_", strings.namespace);
    }
    if (strings.subsystem) {
        fprintf(w, "%s_", strings.subsystem);
    }
    fprintf(w, "%s", strings.name);
}

void export_help(FILE *w, prom_strings_t strings, const char *type) {
    fprintf(w, "# HELP %s\n", strings.help);
    fprintf(w, "# TYPE ");
    export_strings(w, strings);
    fprintf(w, " %s\n", type);
}

void export_values(FILE *w, _prom_base_collector_t *col) {
    if (!col->_num_labels) {
        export_strings(w, col->_strings);
        fprintf(w, " %f\n", col->_values[0]);
        return;
    }

    for (size_t j = 0; j < PROM_MAX_CARDINALITY; j++) {
        if (!col->_label_values[j][0]) {
            break;
        }
        export_strings(w, col->_strings);
        fprintf(w, "{");
        const char *label_val = col->_label_values[j];
        for (size_t k = 0; k < col->_num_labels; k++) {
            fprintf(w, "%s=\"%s\"", col->_labels[k], label_val);
            label_val += strlen(label_val)+1;
            if (k < col->_num_labels-1) {
                fprintf(w, ", ");
            }
        }
        fprintf(w, "} %f\n", col->_values[j]);
    }
}

void prom_registry_export(prom_registry_t *registry, FILE *w) {
    for (size_t i = 0; i < PROM_REGISTRY_MAX_COLLECTORS; i++) {
        collector_t col = registry->_collectors[i];
        if (!col._type) {
            break;
        }

        if (col._type == PROM_COLLECTOR_COUNTER) {
            prom_counter_t *counter = ((prom_counter_t*)col._col);
            export_help(w, counter->_strings, "counter");
            export_values(w, counter);

        } else if (col._type == PROM_COLLECTOR_GAUGE) {
            prom_gauge_t *gauge = ((prom_gauge_t*)col._col);
            export_help(w, gauge->_strings, "gauge");
            export_values(w, gauge);

        } else if (col._type == PROM_COLLECTOR_SUMMARY) {
            assert(0);
        } else if (col._type == PROM_COLLECTOR_HISTOGRAM) {
            assert(0);
        } else {
            assert(0);
        }

        fprintf(w, "\n");
    }
}
