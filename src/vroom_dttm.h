#pragma once

#include "DateTimeParser.h"
#include "parallel.h"

double parse_dttm(
    const std::string& str, DateTimeParser& parser, const std::string& format) {
  parser.setDate(str.c_str());
  bool res = (format == "") ? parser.parseISO8601() : parser.parse(format);

  if (res) {
    DateTime dt = parser.makeDateTime();
    if (dt.validDateTime()) {
      return dt.datetime();
    }
  }
  return NA_REAL;
}

Rcpp::NumericVector read_dttm(vroom_vec_info* info, const std::string& format) {
  R_xlen_t n = info->idx->num_rows();

  Rcpp::NumericVector out(n);

  DateTimeParser parser(&*info->locale);
  R_xlen_t i = 0;
  // parallel_for(
  // n,
  //[&](size_t start, size_t end, size_t id) {
  // auto i = start;
  // DateTimeParser parser(&*info->locale);
  for (const auto& str : info->idx->get_column(info->column)) {
    out[i++] = parse_dttm(str, parser, format);
  }
  //}
  //,
  // info->num_threads,
  // true);

  out.attr("class") = Rcpp::CharacterVector::create("POSIXct", "POSIXt");
  out.attr("tzone") = info->locale->tz_;

  return out;
}

#ifdef HAS_ALTREP

/* Vroom dttm */

struct vroom_dttm_info {
  vroom_vec_info* info;
  std::unique_ptr<DateTimeParser> parser;
  std::string format;
};

class vroom_dttm : public vroom_vec {

public:
  static R_altrep_class_t class_t;

  static SEXP Make(vroom_vec_info* info, const std::string& format) {

    vroom_dttm_info* dttm_info = new vroom_dttm_info;
    dttm_info->info = info;
    dttm_info->parser =
        std::unique_ptr<DateTimeParser>(new DateTimeParser(&*info->locale));
    dttm_info->format = format;

    SEXP out = PROTECT(R_MakeExternalPtr(dttm_info, R_NilValue, R_NilValue));
    R_RegisterCFinalizerEx(out, vroom_dttm::Finalize, FALSE);

    RObject res = R_new_altrep(class_t, out, R_NilValue);

    res.attr("class") = Rcpp::CharacterVector::create("POSIXct", "POSIXt");
    res.attr("tzone") = info->locale->tz_;

    UNPROTECT(1);

    return res;
  }

  // ALTREP methods -------------------

  // What gets printed when .Internal(inspect()) is used
  static Rboolean Inspect(
      SEXP x,
      int pre,
      int deep,
      int pvec,
      void (*inspect_subtree)(SEXP, int, int, int)) {
    Rprintf(
        "vroom_dttm (len=%d, materialized=%s)\n",
        Length(x),
        R_altrep_data2(x) != R_NilValue ? "T" : "F");
    return TRUE;
  }

  static void Finalize(SEXP xp) {
    auto info_p = static_cast<vroom_dttm_info*>(R_ExternalPtrAddr(xp));

    delete info_p->info;
    delete info_p;
  }

  static inline vroom_dttm_info* Info(SEXP x) {
    return static_cast<vroom_dttm_info*>(R_ExternalPtrAddr(R_altrep_data1(x)));
  }

  static inline R_xlen_t Length(SEXP vec) {
    auto inf = Info(vec);
    return inf->info->idx->num_rows();
  }

  static inline std::string Get(SEXP vec, R_xlen_t i) {
    auto inf = Info(vec);
    return inf->info->idx->get(i, inf->info->column);
  }

  // ALTREAL methods -----------------

  // the element at the index `i`
  static double dttm_Elt(SEXP vec, R_xlen_t i) {
    SEXP data2 = R_altrep_data2(vec);
    if (data2 != R_NilValue) {
      return REAL(data2)[i];
    }

    auto str = Get(vec, i);
    auto inf = Info(vec);

    return parse_dttm(str, *inf->parser, inf->format);
  }

  // --- Altvec
  static SEXP Materialize(SEXP vec) {
    SEXP data2 = R_altrep_data2(vec);
    if (data2 != R_NilValue) {
      return data2;
    }

    auto inf = Info(vec);

    auto out = read_dttm(inf->info, inf->format);

    R_set_altrep_data2(vec, out);

    return out;
  }

  static void* Dataptr(SEXP vec, Rboolean writeable) {
    return STDVEC_DATAPTR(Materialize(vec));
  }

  // -------- initialize the altrep class with the methods above
  static void Init(DllInfo* dll) {
    vroom_dttm::class_t = R_make_altreal_class("vroom_dttm", "vroom", dll);

    // altrep
    R_set_altrep_Length_method(class_t, Length);
    R_set_altrep_Inspect_method(class_t, Inspect);

    // altvec
    R_set_altvec_Dataptr_method(class_t, Dataptr);
    R_set_altvec_Dataptr_or_null_method(class_t, Dataptr_or_null);

    // altreal
    R_set_altreal_Elt_method(class_t, dttm_Elt);
  }
};

R_altrep_class_t vroom_dttm::class_t;

// Called the package is loaded (needs Rcpp 0.12.18.3)
// [[Rcpp::init]]
void init_vroom_dttm(DllInfo* dll) { vroom_dttm::Init(dll); }

#else
void init_vroom_dttm(DllInfo* dll) {}
#endif
