#include "reportfmt.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char **cells;
} rf_row_t;

struct rf_ctx {
    rf_format_t fmt;
    FILE* out;            // destino activo
    FILE* out_provided;   // puntero original (no cerrar)
    char* out_path_dup;   // si se abrió archivo, recordar path
    size_t ncols;

    // headers
    char **headers;

    // buffer de filas (para TABLE hacemos width pass al final)
    rf_row_t *rows;
    size_t nrows;
    size_t cap;

    // flags estado (para JSON, saber si ya imprimimos una fila)
    bool first_row_emitted;
};

static char* rf_strdup(const char* s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char* p = (char*)malloc(n + 1u);
    if (!p) return NULL;
    memcpy(p, s, n + 1u);
    return p;
}

static void rf_free_row(rf_row_t* r, size_t ncols) {
    if (!r || !r->cells) return;
    for (size_t i=0;i<ncols;i++) free(r->cells[i]);
    free(r->cells);
}

static bool rf_reserve_rows(struct rf_ctx* c, size_t need) {
    if (c->cap >= need) return true;
    size_t newcap = c->cap ? c->cap * 2u : 16u;
    if (newcap < need) newcap = need;
    rf_row_t* nr = (rf_row_t*)realloc(c->rows, newcap * sizeof(rf_row_t));
    if (!nr) return false;
    c->rows = nr;
    c->cap  = newcap;
    return true;
}

static void rf_json_escape(FILE* f, const char* s) {
    fputc('"', f);
    for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
        unsigned char ch = *p;
        switch (ch) {
            case '\\': fputs("\\\\", f); break;
            case '\"': fputs("\\\"", f); break;
            case '\b': fputs("\\b",  f); break;
            case '\f': fputs("\\f",  f); break;
            case '\n': fputs("\\n",  f); break;
            case '\r': fputs("\\r",  f); break;
            case '\t': fputs("\\t",  f); break;
            default:
                if (ch < 0x20) {
                    fprintf(f, "\\u%04x", ch);
                } else {
                    fputc(ch, f);
                }
        }
    }
    fputc('"', f);
}

static void rf_csv_escape(FILE* f, const char* s) {
    bool need_quotes = false;
    for (const char* p=s; *p; ++p) {
        if (*p == ',' || *p == '"' || *p == '\n' || *p=='\r') { need_quotes = true; break; }
    }
    if (!need_quotes && s[0] != ' ') {
        fputs(s, f);
        return;
    }
    fputc('"', f);
    for (const char* p=s; *p; ++p) {
        if (*p == '"') fputc('"', f); // escape doble comilla
        fputc(*p, f);
    }
    fputc('"', f);
}

rf_format_t rf_format_from_str(const char* s) {
    if (!s) return RF_TABLE;
    // case-insensitive
    char buf[16]; size_t n = 0;
    for (; s[n] && n < sizeof(buf)-1; ++n) buf[n] = (char)tolower((unsigned char)s[n]);
    buf[n] = 0;
    if (strcmp(buf,"json")==0) return RF_JSON;
    if (strcmp(buf,"csv")==0)  return RF_CSV;
    return RF_TABLE;
}

const char* rf_format_name(rf_format_t f) {
    switch (f) {
        case RF_TABLE: return "table";
        case RF_JSON:  return "json";
        case RF_CSV:   return "csv";
        default:       return "table";
    }
}

rf_ctx* rf_begin(rf_format_t fmt,
                 FILE* out,
                 const char* out_path,
                 const char* const * headers,
                 size_t ncols)
{
    if (!out) out = stdout;

    struct rf_ctx* c = (struct rf_ctx*)calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->fmt = fmt;
    c->out = out;
    c->out_provided = out;
    c->ncols = ncols;
    c->first_row_emitted = false;

    // duplicate headers
    c->headers = (char**)calloc(ncols, sizeof(char*));
    if (!c->headers) { free(c); return NULL; }
    for (size_t i=0;i<ncols;i++) {
        c->headers[i] = rf_strdup(headers && headers[i] ? headers[i] : "");
        if (!c->headers[i]) { // free partial
            for (size_t j=0;j<i;j++) free(c->headers[j]);
            free(c->headers); free(c); return NULL;
        }
    }

    // open file if JSON/CSV and out_path provided
    if (out_path && *out_path && (fmt == RF_JSON || fmt == RF_CSV)) {
        FILE* f = fopen(out_path, "wb");
        if (!f) {
            // fallback to provided 'out' but keep going
            c->out = out;
        } else {
            c->out = f;
            c->out_path_dup = rf_strdup(out_path);
        }
    }

    // prologo por formato
    if (fmt == RF_JSON) {
        fputs("[\n", c->out);
    } else if (fmt == RF_CSV) {
        // header row
        for (size_t i=0;i<ncols;i++) {
            if (i) fputc(',', c->out);
            rf_csv_escape(c->out, c->headers[i]);
        }
        fputc('\n', c->out);
    }
    // TABLE: nada (se calcula widths al final)

    return c;
}

bool rf_row(rf_ctx* c, const char* const * cells)
{
    if (!c || !cells) return false;

    if (c->fmt == RF_JSON) {
        if (c->first_row_emitted) fputs(",\n", c->out);
        fputs("  {", c->out);
        for (size_t i=0;i<c->ncols;i++) {
            if (i) fputs(", ", c->out);
            rf_json_escape(c->out, c->headers[i]);
            fputs(": ", c->out);
            rf_json_escape(c->out, cells[i] ? cells[i] : "");
        }
        fputs("}", c->out);
        c->first_row_emitted = true;
        return true;
    } else if (c->fmt == RF_CSV) {
        for (size_t i=0;i<c->ncols;i++) {
            if (i) fputc(',', c->out);
            rf_csv_escape(c->out, cells[i] ? cells[i] : "");
        }
        fputc('\n', c->out);
        return true;
    } else {
        // TABLE: almacenamos para width pass
        if (!rf_reserve_rows(c, c->nrows + 1u)) return false;
        rf_row_t *r = &c->rows[c->nrows++];
        r->cells = (char**)calloc(c->ncols, sizeof(char*));
        if (!r->cells) return false;
        for (size_t i=0;i<c->ncols;i++) {
            r->cells[i] = rf_strdup(cells[i] ? cells[i] : "");
            if (!r->cells[i]) return false;
        }
        return true;
    }
}

static void rf_table_flush(struct rf_ctx* c) {
    // calcular ancho por columna
    size_t *w = (size_t*)calloc(c->ncols, sizeof(size_t));
    if (!w) return;

    for (size_t i=0;i<c->ncols;i++) {
        w[i] = strlen(c->headers[i]);
    }
    for (size_t r=0;r<c->nrows;r++) {
        for (size_t i=0;i<c->ncols;i++) {
            size_t len = c->rows[r].cells[i] ? strlen(c->rows[r].cells[i]) : 0u;
            if (len > w[i]) w[i] = len;
        }
    }

    // separador
    #define SEP_CHAR '-'
    #define PAD "  "

    // header
    for (size_t i=0;i<c->ncols;i++) {
        if (i) fputs("  ", c->out);
        fprintf(c->out, "%-*s", (int)w[i], c->headers[i]);
    }
    fputc('\n', c->out);
    // underline
    for (size_t i=0;i<c->ncols;i++) {
        if (i) fputs("  ", c->out);
        for (size_t k=0;k<w[i];k++) fputc(SEP_CHAR, c->out);
    }
    fputc('\n', c->out);

    // rows
    for (size_t r=0;r<c->nrows;r++) {
        for (size_t i=0;i<c->ncols;i++) {
            if (i) fputs("  ", c->out);
            const char* cell = c->rows[r].cells[i] ? c->rows[r].cells[i] : "";
            fprintf(c->out, "%-*s", (int)w[i], cell);
        }
        fputc('\n', c->out);
    }
    free(w);
}

bool rf_end(rf_ctx* c)
{
    if (!c) return false;
    bool ok = true;

    if (c->fmt == RF_JSON) {
        fputs("\n]\n", c->out);
    } else if (c->fmt == RF_CSV) {
        // nada extra
    } else {
        rf_table_flush(c);
    }

    // liberar
    for (size_t i=0;i<c->ncols;i++) free(c->headers[i]);
    free(c->headers);
    if (c->rows) {
        for (size_t r=0;r<c->nrows;r++) rf_free_row(&c->rows[r], c->ncols);
        free(c->rows);
    }

    // cerrar archivo si lo abrimos nosotros
    if (c->out != c->out_provided && c->out) {
        if (fclose(c->out) != 0) ok = false;
    }
    free(c->out_path_dup);
    free(c);
    return ok;
}

