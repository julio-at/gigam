#ifndef REPORTFMT_H
#define REPORTFMT_H

// Minimal, warning-free C11 formatting sink for table/json/csv.
// Designed to avoid breaking existing code: push headers, push rows, end.

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RF_TABLE = 0,
    RF_JSON  = 1,
    RF_CSV   = 2
} rf_format_t;

// Opaque context
typedef struct rf_ctx rf_ctx_t;

// Crear el sink. Si out_path != NULL y fmt es JSON o CSV, el módulo intentará abrir ese archivo.
// Para TABLE, se ignora out_path y se usa siempre 'out' (típicamente stdout).
// Si out_path es NULL, se escribe en 'out'.
rf_ctx_t* rf_begin(rf_format_t fmt,
                   FILE* out,
                   const char* out_path,
                   const char* const * headers,
                   size_t ncols);

// Agregar una fila (array de ncols strings, ya formateados). Se copian internamente.
bool rf_row(rf_ctx_t* ctx, const char* const * cells);

// Terminar. Cierra archivo si lo abrió internamente. Devuelve true si todo OK.
bool rf_end(rf_ctx_t* ctx);

// Helpers
rf_format_t rf_format_from_str(const char* s);   // "table" | "json" | "csv" (default table)
const char* rf_format_name(rf_format_t f);

#ifdef __cplusplus
}
#endif

#endif // REPORTFMT_H

