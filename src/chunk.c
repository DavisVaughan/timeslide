#include "timeslide.h"
#include "utils.h"

// Helpers defined at the bottom of the file
int pull_every(SEXP every);
void validate_every(int every);
void validate_origin(SEXP origin);
double origin_to_days_from_epoch(SEXP origin);
double origin_to_seconds_from_epoch(SEXP origin);

// -----------------------------------------------------------------------------

SEXP warp_chunk(SEXP x, enum timeslide_chunk_type type, int every, SEXP origin);

// [[ register() ]]
SEXP timeslide_warp_chunk(SEXP x, SEXP by, SEXP every, SEXP origin) {
  enum timeslide_chunk_type type = as_chunk_type(by);
  int every_ = pull_every(every);
  return warp_chunk(x, type, every_, origin);
}

// -----------------------------------------------------------------------------

static SEXP warp_chunk_year(SEXP x, int every, SEXP origin);
static SEXP warp_chunk_month(SEXP x, int every, SEXP origin);
static SEXP warp_chunk_day(SEXP x, int every, SEXP origin);
static SEXP warp_chunk_hour(SEXP x, int every, SEXP origin);
static SEXP warp_chunk_minute(SEXP x, int every, SEXP origin);
static SEXP warp_chunk_second(SEXP x, int every, SEXP origin);

// [[ include("timeslide.h") ]]
SEXP warp_chunk(SEXP x, enum timeslide_chunk_type type, int every, SEXP origin) {
  validate_origin(origin);
  validate_every(every);

  if (time_class_type(x) == timeslide_class_unknown) {
    r_error("warp_chunk", "`x` must inherit from 'Date', 'POSIXct', or 'POSIXlt'.");
  }

  const char* origin_timezone = get_timezone(origin);
  x = PROTECT(convert_timezone(x, origin_timezone));

  SEXP out;

  switch (type) {
  case timeslide_chunk_year: out = PROTECT(warp_chunk_year(x, every, origin)); break;
  case timeslide_chunk_month: out = PROTECT(warp_chunk_month(x, every, origin)); break;
  case timeslide_chunk_day: out = PROTECT(warp_chunk_day(x, every, origin)); break;
  case timeslide_chunk_hour: out = PROTECT(warp_chunk_hour(x, every, origin)); break;
  case timeslide_chunk_minute: out = PROTECT(warp_chunk_minute(x, every, origin)); break;
  case timeslide_chunk_second: out = PROTECT(warp_chunk_second(x, every, origin)); break;
  default: r_error("warp_chunk", "Internal error: unknown `type`.");
  }

  UNPROTECT(2);
  return out;
}

// -----------------------------------------------------------------------------

#define EPOCH_YEAR 1970
#define EPOCH_MONTH 0

static SEXP warp_chunk_year(SEXP x, int every, SEXP origin) {
  int n_prot = 0;

  int origin_year = EPOCH_YEAR;

  if (origin != R_NilValue) {
    SEXP origin_time_df = PROTECT_N(time_get(origin, strings_year), &n_prot);
    origin_year = INTEGER(VECTOR_ELT(origin_time_df, 0))[0];

    if (origin_year == NA_INTEGER) {
      r_error("warp_chunk_year", "`origin` cannot be `NA`.");
    }
  }

  bool needs_every = (every != 1);

  SEXP time_df = PROTECT_N(time_get(x, strings_year), &n_prot);
  SEXP out = VECTOR_ELT(time_df, 0);

  out = PROTECT_N(r_maybe_duplicate(out), &n_prot);
  int* p_out = INTEGER(out);

  R_xlen_t n_out = Rf_xlength(out);

  for (R_xlen_t i = 0; i < n_out; ++i) {
    int elt = p_out[i];

    if (elt == NA_INTEGER) {
      continue;
    }

    elt -= origin_year;

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      p_out[i] = (elt - (every - 1)) / every;
    } else {
      p_out[i] = elt / every;
    }
  }

  UNPROTECT(n_prot);
  return out;
}

// -----------------------------------------------------------------------------

#define MONTHS_IN_YEAR 12

static SEXP warp_chunk_month(SEXP x, int every, SEXP origin) {
  int n_prot = 0;

  int origin_year = EPOCH_YEAR;
  int origin_month = EPOCH_MONTH;

  if (origin != R_NilValue) {
    SEXP origin_time_df = PROTECT_N(time_get(origin, strings_year_month), &n_prot);
    origin_year = INTEGER(VECTOR_ELT(origin_time_df, 0))[0];
    origin_month = INTEGER(VECTOR_ELT(origin_time_df, 1))[0] - 1;

    if (origin_year == NA_INTEGER) {
      r_error("warp_chunk_month", "`origin` cannot be `NA`.");
    }
  }

  bool needs_every = (every != 1);

  SEXP time_df = PROTECT_N(time_get(x, strings_year_month), &n_prot);

  SEXP year = VECTOR_ELT(time_df, 0);
  SEXP month = VECTOR_ELT(time_df, 1);

  const int* p_year = INTEGER_RO(year);
  const int* p_month = INTEGER_RO(month);

  R_xlen_t size = Rf_xlength(year);

  SEXP out = PROTECT_N(Rf_allocVector(INTSXP, size), &n_prot);
  int* p_out = INTEGER(out);

  for (R_xlen_t i = 0; i < size; ++i) {
    int elt_year = p_year[i];
    int elt_month = p_month[i] - 1;

    if (elt_year == NA_INTEGER) {
      p_out[i] = NA_INTEGER;
      continue;
    }

    int elt = (elt_year - origin_year) * MONTHS_IN_YEAR + (elt_month - origin_month);

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(n_prot);
  return out;
}

#undef EPOCH_YEAR
#undef EPOCH_MONTH
#undef MONTHS_IN_YEAR

// -----------------------------------------------------------------------------

static SEXP date_warp_chunk_day(SEXP x, int every, SEXP origin);
static SEXP posixct_warp_chunk_day(SEXP x, int every, SEXP origin);
static SEXP posixlt_warp_chunk_day(SEXP x, int every, SEXP origin);

static SEXP warp_chunk_day(SEXP x, int every, SEXP origin) {
  switch (time_class_type(x)) {
  case timeslide_class_date: return date_warp_chunk_day(x, every, origin);
  case timeslide_class_posixct: return posixct_warp_chunk_day(x, every, origin);
  case timeslide_class_posixlt: return posixlt_warp_chunk_day(x, every, origin);
  default: r_error("warp_chunk_day", "Unknown object with type, %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_date_warp_chunk_day(SEXP x, int every, SEXP origin);
static SEXP dbl_date_warp_chunk_day(SEXP x, int every, SEXP origin);

static SEXP date_warp_chunk_day(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_date_warp_chunk_day(x, every, origin);
  case REALSXP: return dbl_date_warp_chunk_day(x, every, origin);
  default: r_error("date_warp_chunk_day", "Unknown `Date` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_posixct_warp_chunk_day(SEXP x, int every, SEXP origin);
static SEXP dbl_posixct_warp_chunk_day(SEXP x, int every, SEXP origin);

static SEXP posixct_warp_chunk_day(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_posixct_warp_chunk_day(x, every, origin);
  case REALSXP: return dbl_posixct_warp_chunk_day(x, every, origin);
  default: r_error("posixct_warp_chunk_day", "Unknown `POSIXct` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP posixlt_warp_chunk_day(SEXP x, int every, SEXP origin) {
  x = PROTECT(as_datetime(x));
  SEXP out = PROTECT(posixct_warp_chunk_day(x, every, origin));

  UNPROTECT(2);
  return out;
}


static SEXP int_date_warp_chunk_day(SEXP x, int every, SEXP origin) {
  SEXP out = PROTECT(r_maybe_duplicate(x));
  SET_ATTRIB(out, R_NilValue);
  SET_OBJECT(out, 0);

  int* p_out = INTEGER(out);
  R_xlen_t out_size = Rf_xlength(out);

  bool needs_every = (every != 1);
  bool needs_offset = (origin != R_NilValue);

  // Early exit if no changes are required, the raw `day` is enough
  if (!needs_every && !needs_offset) {
    UNPROTECT(1);
    return out;
  }

  double origin_offset;
  if (needs_offset) {
    origin_offset = origin_to_days_from_epoch(origin);
  }

  for (R_xlen_t i = 0; i < out_size; ++i) {
    int elt = p_out[i];

    if (elt == NA_INTEGER) {
      continue;
    }

    if (needs_offset) {
      elt -= origin_offset;
    }

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(1);
  return out;
}

static SEXP dbl_date_warp_chunk_day(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  double* p_x = REAL(x);

  SEXP out = PROTECT(Rf_allocVector(INTSXP, x_size));
  int* p_out = INTEGER(out);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  double origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_days_from_epoch(origin);
  }

  for (R_xlen_t i = 0; i < x_size; ++i) {
    double x_elt = p_x[i];

    if (!R_FINITE(x_elt)) {
      p_out[i] = NA_INTEGER;
      continue;
    }

    // `origin_offset` should be correct from `as_date()` in
    // `origin_to_days_from_epoch()`, even if it had fractional parts
    if (needs_offset) {
      x_elt -= origin_offset;
    }

    int elt;

    // The floor is a guard against potential fractional dates since casting
    // always goes towards 0, which is not desired because that is forward in time
    // (int) 1.5 = 1
    // (int) floor(1.5) = 1
    // (int) -1.5 = -1
    // (int) floor(-1.5) = -2

    // Automatic integer cast into `elt`
    if (x_elt < 0) {
      elt = floor(x_elt);
    } else {
      elt = x_elt;
    }

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(1);
  return out;
}

#define SECONDS_IN_DAY 86400

static SEXP int_posixct_warp_chunk_day(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  double origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_seconds_from_epoch(origin);
  }

  SEXP out = PROTECT(Rf_allocVector(INTSXP, x_size));
  int* p_out = INTEGER(out);

  int* p_x = INTEGER(x);

  for (R_xlen_t i = 0; i < x_size; ++i) {
    int elt = p_x[i];

    if (elt == NA_INTEGER) {
      p_out[i] = NA_INTEGER;
      continue;
    }

    if (needs_offset) {
      elt -= origin_offset;
    }

    // Integer division, then straight into `elt` with no cast needed
    if (elt < 0) {
      elt = (elt - (SECONDS_IN_DAY - 1)) / SECONDS_IN_DAY;
    } else {
      elt = elt / SECONDS_IN_DAY;
    }

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(1);
  return out;
}

static SEXP dbl_posixct_warp_chunk_day(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  double origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_seconds_from_epoch(origin);
  }

  SEXP out = PROTECT(Rf_allocVector(INTSXP, x_size));
  int* p_out = INTEGER(out);

  double* p_x = REAL(x);

  for (R_xlen_t i = 0; i < x_size; ++i) {
    double x_elt = p_x[i];

    if (!R_FINITE(x_elt)) {
      p_out[i] = NA_INTEGER;
      continue;
    }

    if (needs_offset) {
      x_elt -= origin_offset;
    }

    int elt;

    // Double division, then integer cast into `elt`
    if (x_elt < 0) {
      elt = (floor(x_elt) - (SECONDS_IN_DAY - 1)) / SECONDS_IN_DAY;
    } else {
      elt = x_elt / SECONDS_IN_DAY;
    }

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(1);
  return out;
}

#undef SECONDS_IN_DAY

// -----------------------------------------------------------------------------

static SEXP date_warp_chunk_hour(SEXP x, int every, SEXP origin);
static SEXP posixct_warp_chunk_hour(SEXP x, int every, SEXP origin);
static SEXP posixlt_warp_chunk_hour(SEXP x, int every, SEXP origin);

static SEXP warp_chunk_hour(SEXP x, int every, SEXP origin) {
  switch (time_class_type(x)) {
  case timeslide_class_date: return date_warp_chunk_hour(x, every, origin);
  case timeslide_class_posixct: return posixct_warp_chunk_hour(x, every, origin);
  case timeslide_class_posixlt: return posixlt_warp_chunk_hour(x, every, origin);
  default: r_error("warp_chunk_hour", "Unknown object with type, %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_date_warp_chunk_hour(SEXP x, int every, SEXP origin);
static SEXP dbl_date_warp_chunk_hour(SEXP x, int every, SEXP origin);

static SEXP date_warp_chunk_hour(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_date_warp_chunk_hour(x, every, origin);
  case REALSXP: return dbl_date_warp_chunk_hour(x, every, origin);
  default: r_error("date_warp_chunk_hour", "Unknown `Date` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_posixct_warp_chunk_hour(SEXP x, int every, SEXP origin);
static SEXP dbl_posixct_warp_chunk_hour(SEXP x, int every, SEXP origin);

static SEXP posixct_warp_chunk_hour(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_posixct_warp_chunk_hour(x, every, origin);
  case REALSXP: return dbl_posixct_warp_chunk_hour(x, every, origin);
  default: r_error("posixct_warp_chunk_hour", "Unknown `POSIXct` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP posixlt_warp_chunk_hour(SEXP x, int every, SEXP origin) {
  x = PROTECT(as_datetime(x));
  SEXP out = PROTECT(posixct_warp_chunk_hour(x, every, origin));

  UNPROTECT(2);
  return out;
}


#define HOURS_IN_DAY 24

static SEXP int_date_warp_chunk_hour(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  int* p_x = INTEGER(x);

  SEXP out = PROTECT(Rf_allocVector(INTSXP, x_size));
  int* p_out = INTEGER(out);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  double origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_days_from_epoch(origin) * HOURS_IN_DAY;
  }

  for (R_xlen_t i = 0; i < x_size; ++i) {
    int elt = p_x[i];

    if (elt == NA_INTEGER) {
      p_out[i] = NA_INTEGER;
      continue;
    }

    elt = elt * HOURS_IN_DAY;

    if (needs_offset) {
      elt -= origin_offset;
    }

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(1);
  return out;
}

static SEXP dbl_date_warp_chunk_hour(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  double* p_x = REAL(x);

  SEXP out = PROTECT(Rf_allocVector(INTSXP, x_size));
  int* p_out = INTEGER(out);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  double origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_days_from_epoch(origin) * HOURS_IN_DAY;
  }

  for (R_xlen_t i = 0; i < x_size; ++i) {
    double x_elt = p_x[i];

    if (!R_FINITE(x_elt)) {
      p_out[i] = NA_INTEGER;
      continue;
    }

    x_elt = x_elt * HOURS_IN_DAY;

    // `origin_offset` should be correct from `as_date()` in
    // `origin_to_days_from_epoch()`, even if it had fractional parts
    if (needs_offset) {
      x_elt -= origin_offset;
    }

    int elt;

    // Automatic integer cast into `elt`
    if (x_elt < 0) {
      elt = floor(x_elt);
    } else {
      elt = x_elt;
    }

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(1);
  return out;
}

#undef HOURS_IN_DAY

#define SECONDS_IN_HOUR 3600

static SEXP int_posixct_warp_chunk_hour(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  double origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_seconds_from_epoch(origin);
  }

  SEXP out = PROTECT(Rf_allocVector(INTSXP, x_size));
  int* p_out = INTEGER(out);

  int* p_x = INTEGER(x);

  for (R_xlen_t i = 0; i < x_size; ++i) {
    int elt = p_x[i];

    if (elt == NA_INTEGER) {
      p_out[i] = NA_INTEGER;
      continue;
    }

    if (needs_offset) {
      elt -= origin_offset;
    }

    // Integer division, then straight into `elt` with no cast needed
    if (elt < 0) {
      elt = (elt - (SECONDS_IN_HOUR - 1)) / SECONDS_IN_HOUR;
    } else {
      elt = elt / SECONDS_IN_HOUR;
    }

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(1);
  return out;
}

static SEXP dbl_posixct_warp_chunk_hour(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  double origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_seconds_from_epoch(origin);
  }

  SEXP out = PROTECT(Rf_allocVector(INTSXP, x_size));
  int* p_out = INTEGER(out);

  double* p_x = REAL(x);

  for (R_xlen_t i = 0; i < x_size; ++i) {
    double x_elt = p_x[i];

    if (!R_FINITE(x_elt)) {
      p_out[i] = NA_INTEGER;
      continue;
    }

    if (needs_offset) {
      x_elt -= origin_offset;
    }

    int elt;

    // Double division, then integer cast into `elt`
    if (x_elt < 0) {
      elt = (floor(x_elt) - (SECONDS_IN_HOUR - 1)) / SECONDS_IN_HOUR;
    } else {
      elt = x_elt / SECONDS_IN_HOUR;
    }

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(1);
  return out;
}

#undef SECONDS_IN_HOUR

// -----------------------------------------------------------------------------

static SEXP date_warp_chunk_minute(SEXP x, int every, SEXP origin);
static SEXP posixct_warp_chunk_minute(SEXP x, int every, SEXP origin);
static SEXP posixlt_warp_chunk_minute(SEXP x, int every, SEXP origin);

static SEXP warp_chunk_minute(SEXP x, int every, SEXP origin) {
  switch (time_class_type(x)) {
  case timeslide_class_date: return date_warp_chunk_minute(x, every, origin);
  case timeslide_class_posixct: return posixct_warp_chunk_minute(x, every, origin);
  case timeslide_class_posixlt: return posixlt_warp_chunk_minute(x, every, origin);
  default: r_error("warp_chunk_minute", "Unknown object with type, %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_date_warp_chunk_minute(SEXP x, int every, SEXP origin);
static SEXP dbl_date_warp_chunk_minute(SEXP x, int every, SEXP origin);

static SEXP date_warp_chunk_minute(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_date_warp_chunk_minute(x, every, origin);
  case REALSXP: return dbl_date_warp_chunk_minute(x, every, origin);
  default: r_error("date_warp_chunk_minute", "Unknown `Date` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_posixct_warp_chunk_minute(SEXP x, int every, SEXP origin);
static SEXP dbl_posixct_warp_chunk_minute(SEXP x, int every, SEXP origin);

static SEXP posixct_warp_chunk_minute(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_posixct_warp_chunk_minute(x, every, origin);
  case REALSXP: return dbl_posixct_warp_chunk_minute(x, every, origin);
  default: r_error("posixct_warp_chunk_minute", "Unknown `POSIXct` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP posixlt_warp_chunk_minute(SEXP x, int every, SEXP origin) {
  x = PROTECT(as_datetime(x));
  SEXP out = PROTECT(posixct_warp_chunk_minute(x, every, origin));

  UNPROTECT(2);
  return out;
}


#define MINUTES_IN_DAY 1440

static SEXP int_date_warp_chunk_minute(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  int* p_x = INTEGER(x);

  SEXP out = PROTECT(Rf_allocVector(INTSXP, x_size));
  int* p_out = INTEGER(out);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  double origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_days_from_epoch(origin) * MINUTES_IN_DAY;
  }

  for (R_xlen_t i = 0; i < x_size; ++i) {
    int elt = p_x[i];

    if (elt == NA_INTEGER) {
      p_out[i] = NA_INTEGER;
      continue;
    }

    elt = elt * MINUTES_IN_DAY;

    if (needs_offset) {
      elt -= origin_offset;
    }

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(1);
  return out;
}

static SEXP dbl_date_warp_chunk_minute(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  double* p_x = REAL(x);

  SEXP out = PROTECT(Rf_allocVector(INTSXP, x_size));
  int* p_out = INTEGER(out);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  double origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_days_from_epoch(origin) * MINUTES_IN_DAY;
  }

  for (R_xlen_t i = 0; i < x_size; ++i) {
    double x_elt = p_x[i];

    if (!R_FINITE(x_elt)) {
      p_out[i] = NA_INTEGER;
      continue;
    }

    x_elt = x_elt * MINUTES_IN_DAY;

    // `origin_offset` should be correct from `as_date()` in
    // `origin_to_days_from_epoch()`, even if it had fractional parts
    if (needs_offset) {
      x_elt -= origin_offset;
    }

    int elt;

    // Automatic integer cast into `elt`
    if (x_elt < 0) {
      elt = floor(x_elt);
    } else {
      elt = x_elt;
    }

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(1);
  return out;
}

#undef MINUTES_IN_DAY

#define SECONDS_IN_MINUTE 60

static SEXP int_posixct_warp_chunk_minute(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  double origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_seconds_from_epoch(origin);
  }

  SEXP out = PROTECT(Rf_allocVector(INTSXP, x_size));
  int* p_out = INTEGER(out);

  int* p_x = INTEGER(x);

  for (R_xlen_t i = 0; i < x_size; ++i) {
    int elt = p_x[i];

    if (elt == NA_INTEGER) {
      p_out[i] = NA_INTEGER;
      continue;
    }

    if (needs_offset) {
      elt -= origin_offset;
    }

    // Integer division, then straight into `elt` with no cast needed
    if (elt < 0) {
      elt = (elt - (SECONDS_IN_MINUTE - 1)) / SECONDS_IN_MINUTE;
    } else {
      elt = elt / SECONDS_IN_MINUTE;
    }

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(1);
  return out;
}

static SEXP dbl_posixct_warp_chunk_minute(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  double origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_seconds_from_epoch(origin);
  }

  SEXP out = PROTECT(Rf_allocVector(INTSXP, x_size));
  int* p_out = INTEGER(out);

  double* p_x = REAL(x);

  for (R_xlen_t i = 0; i < x_size; ++i) {
    double x_elt = p_x[i];

    if (!R_FINITE(x_elt)) {
      p_out[i] = NA_INTEGER;
      continue;
    }

    if (needs_offset) {
      x_elt -= origin_offset;
    }

    int elt;

    // Double division, then integer cast into `elt`
    if (x_elt < 0) {
      elt = (floor(x_elt) - (SECONDS_IN_MINUTE - 1)) / SECONDS_IN_MINUTE;
    } else {
      elt = x_elt / SECONDS_IN_MINUTE;
    }

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(1);
  return out;
}

#undef SECONDS_IN_MINUTE

// -----------------------------------------------------------------------------

static SEXP date_warp_chunk_second(SEXP x, int every, SEXP origin);
static SEXP posixct_warp_chunk_second(SEXP x, int every, SEXP origin);
static SEXP posixlt_warp_chunk_second(SEXP x, int every, SEXP origin);

static SEXP warp_chunk_second(SEXP x, int every, SEXP origin) {
  switch (time_class_type(x)) {
  case timeslide_class_date: return date_warp_chunk_second(x, every, origin);
  case timeslide_class_posixct: return posixct_warp_chunk_second(x, every, origin);
  case timeslide_class_posixlt: return posixlt_warp_chunk_second(x, every, origin);
  default: r_error("warp_chunk_second", "Unknown object with type, %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_date_warp_chunk_second(SEXP x, int every, SEXP origin);
static SEXP dbl_date_warp_chunk_second(SEXP x, int every, SEXP origin);

static SEXP date_warp_chunk_second(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_date_warp_chunk_second(x, every, origin);
  case REALSXP: return dbl_date_warp_chunk_second(x, every, origin);
  default: r_error("date_warp_chunk_second", "Unknown `Date` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_posixct_warp_chunk_second(SEXP x, int every, SEXP origin);
static SEXP dbl_posixct_warp_chunk_second(SEXP x, int every, SEXP origin);

static SEXP posixct_warp_chunk_second(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_posixct_warp_chunk_second(x, every, origin);
  case REALSXP: return dbl_posixct_warp_chunk_second(x, every, origin);
  default: r_error("posixct_warp_chunk_second", "Unknown `POSIXct` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP posixlt_warp_chunk_second(SEXP x, int every, SEXP origin) {
  x = PROTECT(as_datetime(x));
  SEXP out = PROTECT(posixct_warp_chunk_second(x, every, origin));

  UNPROTECT(2);
  return out;
}


#define SECONDS_IN_DAY 86400

static SEXP int_date_warp_chunk_second(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  int* p_x = INTEGER(x);

  SEXP out = PROTECT(Rf_allocVector(REALSXP, x_size));
  double* p_out = REAL(out);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  double origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_days_from_epoch(origin) * SECONDS_IN_DAY;
  }

  for (R_xlen_t i = 0; i < x_size; ++i) {
    int x_elt = p_x[i];

    if (x_elt == NA_INTEGER) {
      p_out[i] = NA_REAL;
      continue;
    }

    // Convert to int64_t here to hold `elt * SECONDS_IN_DAY`
    // Can't be double because we still need integer division later
    int64_t elt = x_elt * SECONDS_IN_DAY;

    if (needs_offset) {
      elt -= origin_offset;
    }

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(1);
  return out;
}

static SEXP dbl_date_warp_chunk_second(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  double* p_x = REAL(x);

  SEXP out = PROTECT(Rf_allocVector(REALSXP, x_size));
  double* p_out = REAL(out);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  double origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_days_from_epoch(origin) * SECONDS_IN_DAY;
  }

  for (R_xlen_t i = 0; i < x_size; ++i) {
    double x_elt = p_x[i];

    if (!R_FINITE(x_elt)) {
      p_out[i] = NA_REAL;
      continue;
    }

    x_elt = x_elt * SECONDS_IN_DAY;

    // `origin_offset` should be correct from `as_date()` in
    // `origin_to_days_from_epoch()`, even if it had fractional parts
    if (needs_offset) {
      x_elt -= origin_offset;
    }

    // Always floor, whether negative or positive, to get rid of the
    // fractional piece. `floor()` always goes the right way. Need
    // int64_t for the integer division later
    int64_t elt = floor(x_elt);

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(1);
  return out;
}

#undef SECONDS_IN_DAY

static SEXP int_posixct_warp_chunk_second(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  double origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_seconds_from_epoch(origin);
  }

  SEXP out = PROTECT(Rf_allocVector(REALSXP, x_size));
  double* p_out = REAL(out);

  int* p_x = INTEGER(x);

  for (R_xlen_t i = 0; i < x_size; ++i) {
    // Starts as `int`, since that is what `x` is
    int x_elt = p_x[i];

    if (x_elt == NA_INTEGER) {
      p_out[i] = NA_REAL;
      continue;
    }

    // Convert to `int64_t` in case `elt -= origin_offset` goes OOB
    int64_t elt = x_elt;

    if (needs_offset) {
      elt -= origin_offset;
    }

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(1);
  return out;
}

static SEXP dbl_posixct_warp_chunk_second(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  double origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_seconds_from_epoch(origin);
  }

  SEXP out = PROTECT(Rf_allocVector(REALSXP, x_size));
  double* p_out = REAL(out);

  double* p_x = REAL(x);

  for (R_xlen_t i = 0; i < x_size; ++i) {
    double x_elt = p_x[i];

    if (!R_FINITE(x_elt)) {
      p_out[i] = NA_REAL;
      continue;
    }

    if (needs_offset) {
      x_elt -= origin_offset;
    }

    // Always floor() to get rid of fractional seconds, whether `x_elt` is
    // negative or positive. Need int64_t here because of the integer
    // division later
    int64_t elt = floor(x_elt);

    if (!needs_every) {
      p_out[i] = elt;
      continue;
    }

    if (elt < 0) {
      elt = (elt - (every - 1)) / every;
    } else {
      elt = elt / every;
    }

    p_out[i] = elt;
  }

  UNPROTECT(1);
  return out;
}

// -----------------------------------------------------------------------------

// TODO - Could be lossy...really should use vctrs? Callable from C?
int pull_every(SEXP every) {
  if (Rf_length(every) != 1) {
    r_error("pull_every", "`every` must have size 1, not %i", Rf_length(every));
  }

  switch (TYPEOF(every)) {
  case INTSXP: return INTEGER(every)[0];
  case REALSXP: return Rf_asInteger(every);
  default: r_error("pull_every", "`every` must be integer-ish, not %s", Rf_type2char(TYPEOF(every)));
  }
}

void validate_every(int every) {
  if (every == NA_INTEGER) {
    r_error("validate_every", "`every` must not be `NA`");
  }

  if (every <= 0) {
    r_error("validate_every", "`every` must be an integer greater than 0, not %i", every);
  }
}

void validate_origin(SEXP origin) {
  if (origin == R_NilValue) {
    return;
  }

  R_len_t n_origin = Rf_length(origin);

  if (n_origin != 1) {
    r_error("validate_origin", "`origin` must have size 1, not %i.", n_origin);
  }

  if (time_class_type(origin) == timeslide_class_unknown) {
    r_error("validate_origin", "`origin` must inherit from 'Date', 'POSIXct', or 'POSIXlt'.");
  }
}

double origin_to_days_from_epoch(SEXP origin) {
  origin = PROTECT(as_date(origin));

  double out = REAL(origin)[0];

  if (out == NA_REAL) {
    r_error("origin_to_days_from_epoch", "`origin` must not be `NA`.");
  }

  UNPROTECT(1);
  return out;
}

double origin_to_seconds_from_epoch(SEXP origin) {
  origin = PROTECT(as_datetime(origin));

  double out = REAL(origin)[0];

  if (out == NA_REAL) {
    r_error("origin_to_seconds_from_epoch", "`origin` must not be `NA`.");
  }

  UNPROTECT(1);
  return out;
}
