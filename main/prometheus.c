#include "prometheus.h"
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
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

void prom_counter_init(prom_counter_t *counter, prom_strings_t strings) {
    counter->_strings = strings;
    counter->_count   = 0.0;
}

void prom_counter_inc(prom_counter_t *counter) {
    prom_counter_inc_v(counter, 1.0);
}

void prom_counter_inc_v(prom_counter_t *counter, double v) {
    assert(v >= 0.0);
    counter->_count += v;
}


void prom_gauge_init(prom_gauge_t *gauge, prom_strings_t strings) {
    gauge->_strings = strings;
    prom_gauge_init_vec(gauge, strings, NULL, 0);
}

void prom_gauge_init_vec(prom_gauge_t *gauge, prom_strings_t strings,
    const char **labels, size_t num_labels) {
    gauge->_strings = strings;

    memset(&gauge->_values, 0, sizeof(gauge->_values));
    memcpy(gauge->_labels, labels, num_labels*sizeof(char*));
    gauge->_num_labels = num_labels;
    memset((void*)&gauge->_label_values, 0, sizeof(gauge->_label_values));
}

void prom_gauge_inc(prom_gauge_t *gauge) {
    prom_gauge_inc_v(gauge, 1.0);
}

void prom_gauge_inc_v(prom_gauge_t *gauge, double v) {
    gauge->_values[0] += v;
}

void prom_gauge_dec(prom_gauge_t *gauge) {
    prom_gauge_inc_v(gauge, -1.0);
}

void prom_gauge_dec_v(prom_gauge_t *gauge, double v) {
    gauge->_values[0] -= v;
}

void prom_gauge_set(prom_gauge_t *gauge, double v, ...) {
    if (!gauge->_num_labels) {
        gauge->_values[0] = v;
        return;
    }

    va_list argp;
    va_start(argp, v);

    char joined[PROM_MAX_LABEL_VALUES_LENGTH] = { 0 };
    char *join_ptr = &joined[0];
    for (size_t i = 0; i < gauge->_num_labels; i++) {
        const char *l = va_arg(argp, const char*);
        assert(l);
        strncpy(join_ptr, l, (&joined[0]+sizeof(joined)) - join_ptr);
        join_ptr += strlen(l);
        *join_ptr = 0;
        join_ptr += 1;
    }

    va_end(argp);

    int slot = -1;
    for (size_t i = 0; i < PROM_MAX_CARDINALITY; i++) {
        if (!gauge->_label_values[i][0]) {
            memcpy(gauge->_label_values[i], joined, PROM_MAX_LABEL_VALUES_LENGTH);
            slot = i;
            break;
        }
        if (!strncmp((const char*)&joined, gauge->_label_values[i], PROM_MAX_LABEL_VALUES_LENGTH)) {
            slot = i;
            break;
        }
    }
    assert(slot >= 0);
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

int prom_format(prom_registry_t *registry, char *buf, size_t buf_len) {
    int offset = 0;

#define BUF_WRITE(fmt, args...) do {\
    offset += snprintf(buf+offset, buf_len-offset, fmt, args);\
    if (offset > buf_len) return buf_len-offset;\
} while (0)

    for (size_t i = 0; i < PROM_REGISTRY_MAX_COLLECTORS; i++) {
        collector_t col = registry->_collectors[i];
        if (!col._type) {
            break;
        }

        if (col._type == PROM_COLLECTOR_COUNTER) {
            prom_counter_t *counter = ((prom_counter_t*)col._col);
            BUF_WRITE("# HELP %s\n", counter->_strings.help);
            BUF_WRITE("# TYPE %s_%s %s\n", counter->_strings.subsystem, counter->_strings.name, "counter");
            BUF_WRITE("%s_%s %f\n", counter->_strings.subsystem, counter->_strings.name, counter->_count);

        } else if (col._type == PROM_COLLECTOR_GAUGE) {
            prom_gauge_t *gauge = ((prom_gauge_t*)col._col);
            BUF_WRITE("# HELP %s\n", gauge->_strings.help);
            BUF_WRITE("# TYPE %s_%s %s\n", gauge->_strings.subsystem, gauge->_strings.name, "gauge");
            if (!gauge->_num_labels) {
                BUF_WRITE("%s_%s %f\n", gauge->_strings.subsystem, gauge->_strings.name, gauge->_values[0]);

            } else {
                for (size_t j = 0; j < PROM_MAX_CARDINALITY; j++) {
                    if (!gauge->_label_values[j][0]) {
                        break;
                    }
                    BUF_WRITE("%s_%s{", gauge->_strings.subsystem, gauge->_strings.name);
                    const char *label_val = gauge->_label_values[j];
                    for (size_t k = 0; k < gauge->_num_labels; k++) {
                        BUF_WRITE("%s=\"%s\"", gauge->_labels[k], label_val);
                        label_val += strlen(label_val)+1;
                        if (k < gauge->_num_labels-1) {
                            BUF_WRITE("%s", ", ");
                        }
                    }
                    BUF_WRITE("} %f\n", gauge->_values[j]);
                }
            }

        } else if (col._type == PROM_COLLECTOR_SUMMARY) {
            assert(0);
        } else if (col._type == PROM_COLLECTOR_HISTOGRAM) {
            assert(0);
        } else {
            assert(0);
        }

        BUF_WRITE("%s", "\n");
    }

#undef BUF_WRITE

    buf[offset] = 0;
    return offset;
}
