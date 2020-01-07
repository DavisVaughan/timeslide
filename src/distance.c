#include "warp.h"
#include "utils.h"
#include "divmod.h"
#include <stdint.h> // For int64_t (especially on Windows)

// Helpers defined at the bottom of the file
static void validate_every(int every);
static void validate_origin(SEXP origin);
static int origin_to_days_from_epoch(SEXP origin);
static int64_t origin_to_seconds_from_epoch(SEXP origin);
static int64_t origin_to_milliseconds_from_epoch(SEXP origin);
static inline int64_t guarded_floor(double x);
static inline int64_t guarded_floor_to_millisecond(double x);

// -----------------------------------------------------------------------------

static SEXP warp_distance_year(SEXP x, int every, SEXP origin);
static SEXP warp_distance_quarter(SEXP x, int every, SEXP origin);
static SEXP warp_distance_month(SEXP x, int every, SEXP origin);
static SEXP warp_distance_week(SEXP x, int every, SEXP origin);
static SEXP warp_distance_yweek(SEXP x, int every, SEXP origin);
static SEXP warp_distance_day(SEXP x, int every, SEXP origin);
static SEXP warp_distance_yday(SEXP x, int every, SEXP origin);
static SEXP warp_distance_hour(SEXP x, int every, SEXP origin);
static SEXP warp_distance_minute(SEXP x, int every, SEXP origin);
static SEXP warp_distance_second(SEXP x, int every, SEXP origin);
static SEXP warp_distance_millisecond(SEXP x, int every, SEXP origin);

// [[ include("warp.h") ]]
SEXP warp_distance(SEXP x, enum warp_period_type type, int every, SEXP origin) {
  validate_origin(origin);
  validate_every(every);

  if (time_class_type(x) == warp_class_unknown) {
    r_error("warp_distance", "`x` must inherit from 'Date', 'POSIXct', or 'POSIXlt'.");
  }

  if (origin == R_NilValue) {
    origin = PROTECT(get_origin_epoch_in_time_zone(x));
  } else {
    x = PROTECT(convert_time_zone(x, origin));
  }

  SEXP out;

  switch (type) {
  case warp_period_year: out = PROTECT(warp_distance_year(x, every, origin)); break;
  case warp_period_quarter: out = PROTECT(warp_distance_quarter(x, every, origin)); break;
  case warp_period_month: out = PROTECT(warp_distance_month(x, every, origin)); break;
  case warp_period_week: out = PROTECT(warp_distance_week(x, every, origin)); break;
  case warp_period_yweek: out = PROTECT(warp_distance_yweek(x, every, origin)); break;
  case warp_period_day: out = PROTECT(warp_distance_day(x, every, origin)); break;
  case warp_period_yday: out = PROTECT(warp_distance_yday(x, every, origin)); break;
  case warp_period_hour: out = PROTECT(warp_distance_hour(x, every, origin)); break;
  case warp_period_minute: out = PROTECT(warp_distance_minute(x, every, origin)); break;
  case warp_period_second: out = PROTECT(warp_distance_second(x, every, origin)); break;
  case warp_period_millisecond: out = PROTECT(warp_distance_millisecond(x, every, origin)); break;
  default: r_error("warp_distance", "Internal error: unknown `type`.");
  }

  UNPROTECT(2);
  return out;
}

// [[ register() ]]
SEXP warp_warp_distance(SEXP x, SEXP period, SEXP every, SEXP origin) {
  enum warp_period_type type = as_period_type(period);
  int every_ = pull_every(every);
  return warp_distance(x, type, every_, origin);
}

// -----------------------------------------------------------------------------

static SEXP warp_distance_year(SEXP x, int every, SEXP origin) {
  int n_prot = 0;

  bool needs_offset = (origin != R_NilValue);

  int origin_offset;

  if (needs_offset) {
    SEXP origin_offset_sexp = PROTECT_N(get_year_offset(origin), &n_prot);
    origin_offset = INTEGER(origin_offset_sexp)[0];

    if (origin_offset == NA_INTEGER) {
      r_error("warp_distance_year", "`origin` cannot be `NA`.");
    }
  }

  bool needs_every = (every != 1);

  SEXP year = PROTECT_N(get_year_offset(x), &n_prot);
  int* p_year = INTEGER(year);

  R_xlen_t n_out = Rf_xlength(year);

  SEXP out = PROTECT_N(Rf_allocVector(REALSXP, n_out), &n_prot);
  double* p_out = REAL(out);

  for (R_xlen_t i = 0; i < n_out; ++i) {
    int elt = p_year[i];

    if (elt == NA_INTEGER) {
      p_out[i] = NA_REAL;
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
      p_out[i] = (elt - (every - 1)) / every;
    } else {
      p_out[i] = elt / every;
    }
  }

  UNPROTECT(n_prot);
  return out;
}

// -----------------------------------------------------------------------------

static SEXP warp_distance_quarter(SEXP x, int every, SEXP origin) {
  return warp_distance_month(x, every * 3, origin);
}

// -----------------------------------------------------------------------------

static SEXP warp_distance_month(SEXP x, int every, SEXP origin) {
  int n_prot = 0;

  bool needs_offset = (origin != R_NilValue);

  int origin_offset;

  if (needs_offset) {
    SEXP origin_offset_sexp = PROTECT_N(get_month_offset(origin), &n_prot);
    origin_offset = INTEGER(origin_offset_sexp)[0];

    if (origin_offset == NA_INTEGER) {
      r_error("warp_distance_month", "`origin` cannot be `NA`.");
    }
  }

  bool needs_every = (every != 1);

  SEXP month = PROTECT_N(get_month_offset(x), &n_prot);
  const int* p_month = INTEGER_RO(month);

  R_xlen_t size = Rf_xlength(month);

  SEXP out = PROTECT_N(Rf_allocVector(REALSXP, size), &n_prot);
  double* p_out = REAL(out);

  for (R_xlen_t i = 0; i < size; ++i) {
    int elt = p_month[i];

    if (elt == NA_INTEGER) {
      p_out[i] = NA_REAL;
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

  UNPROTECT(n_prot);
  return out;
}

// -----------------------------------------------------------------------------

static SEXP warp_distance_week(SEXP x, int every, SEXP origin) {
  return warp_distance_day(x, every * 7, origin);
}

// -----------------------------------------------------------------------------

static SEXP warp_distance_yweek(SEXP x, int every, SEXP origin) {
  return warp_distance_yday(x, every * 7, origin);
}

// -----------------------------------------------------------------------------

static SEXP warp_distance_day(SEXP x, int every, SEXP origin) {
  int n_prot = 0;

  bool needs_offset = (origin != R_NilValue);

  int origin_offset;

  if (needs_offset) {
    SEXP origin_offset_sexp = PROTECT_N(get_day_offset(origin), &n_prot);
    origin_offset = INTEGER(origin_offset_sexp)[0];

    if (origin_offset == NA_INTEGER) {
      r_error("warp_distance_day", "`origin` cannot be `NA`.");
    }
  }

  bool needs_every = (every != 1);

  SEXP day = PROTECT_N(get_day_offset(x), &n_prot);
  const int* p_day = INTEGER_RO(day);

  R_xlen_t size = Rf_xlength(day);

  SEXP out = PROTECT_N(Rf_allocVector(REALSXP, size), &n_prot);
  double* p_out = REAL(out);

  for (R_xlen_t i = 0; i < size; ++i) {
    int elt = p_day[i];

    if (elt == NA_INTEGER) {
      p_out[i] = NA_REAL;
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

  UNPROTECT(n_prot);
  return out;
}

// -----------------------------------------------------------------------------

static SEXP date_warp_distance_yday(SEXP x, int every, SEXP origin);
static SEXP posixct_warp_distance_yday(SEXP x, int every, SEXP origin);
static SEXP posixlt_warp_distance_yday(SEXP x, int every, SEXP origin);

static SEXP warp_distance_yday(SEXP x, int every, SEXP origin) {
  switch (time_class_type(x)) {
  case warp_class_date: return date_warp_distance_yday(x, every, origin);
  case warp_class_posixct: return posixct_warp_distance_yday(x, every, origin);
  case warp_class_posixlt: return posixlt_warp_distance_yday(x, every, origin);
  default: r_error("warp_distance_yday", "Unknown object with type, %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_date_warp_distance_yday(SEXP x, int every, SEXP origin);
static SEXP dbl_date_warp_distance_yday(SEXP x, int every, SEXP origin);

static SEXP date_warp_distance_yday(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_date_warp_distance_yday(x, every, origin);
  case REALSXP: return dbl_date_warp_distance_yday(x, every, origin);
  default: r_error("date_warp_distance_yday", "Unknown `Date` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP posixct_warp_distance_yday(SEXP x, int every, SEXP origin) {
  x = PROTECT(as_posixlt_from_posixct(x));
  SEXP out = posixlt_warp_distance_yday(x, every, origin);
  UNPROTECT(1);
  return out;
}

#define DAYS_IN_YEAR 365
#define DAYS_IN_LEAP_YEAR 366
#define is_leap_year(year) ((((year) % 4) == 0 && ((year) % 100) != 0) || ((year) % 400) == 0)

static int compute_yday_distance(int days_since_epoch,
                                 int year_offset,
                                 int yday,
                                 int origin_year_offset,
                                 int origin_yday,
                                 int origin_leap,
                                 int units_in_leap_year,
                                 int units_in_non_leap_year,
                                 int every);

static inline int days_before_year(int year_offset);

static SEXP posixlt_warp_distance_yday(SEXP x, int every, SEXP origin) {
  SEXP year = VECTOR_ELT(x, 5);
  SEXP yday = VECTOR_ELT(x, 7);

  if (TYPEOF(year) != INTSXP) {
    r_error(
      "posixlt_warp_distance_yday",
      "Internal error: The 6th element of the POSIXlt object should be an integer."
    );
  }

  if (TYPEOF(yday) != INTSXP) {
    r_error(
      "posixlt_warp_distance_yday",
      "Internal error: The 8th element of the POSIXlt object should be an integer."
    );
  }

  int* p_year = INTEGER(year);
  int* p_yday = INTEGER(yday);

  R_xlen_t size = Rf_xlength(year);

  SEXP out = PROTECT(Rf_allocVector(REALSXP, size));
  double* p_out = REAL(out);

  int units_in_non_leap_year = (DAYS_IN_YEAR - 1) / every + 1;
  int units_in_leap_year = (DAYS_IN_LEAP_YEAR - 1) / every + 1;

  struct warp_yday_components origin_components = get_origin_yday_components(origin);
  int origin_year_offset = origin_components.year_offset;
  int origin_yday = origin_components.yday;
  bool origin_leap = is_leap_year(origin_year_offset + 1970);

  for (R_xlen_t i = 0; i < size; ++i) {
    if (p_year[i] == NA_INTEGER) {
      p_out[i] = NA_REAL;
      continue;
    }

    int year_offset = p_year[i] - 70;
    int yday = p_yday[i];

    int days_since_epoch = days_before_year(year_offset) + yday;

    p_out[i] = compute_yday_distance(
      days_since_epoch,
      year_offset,
      yday,
      origin_year_offset,
      origin_yday,
      origin_leap,
      units_in_leap_year,
      units_in_non_leap_year,
      every
    );
  }

  UNPROTECT(1);
  return out;
}

static SEXP int_date_warp_distance_yday(SEXP x, int every, SEXP origin) {
  int* p_x = INTEGER(x);

  R_xlen_t size = Rf_xlength(x);

  SEXP out = PROTECT(Rf_allocVector(REALSXP, size));
  double* p_out = REAL(out);

  int units_in_non_leap_year = (DAYS_IN_YEAR - 1) / every + 1;
  int units_in_leap_year = (DAYS_IN_LEAP_YEAR - 1) / every + 1;

  struct warp_yday_components origin_components = get_origin_yday_components(origin);
  int origin_year_offset = origin_components.year_offset;
  int origin_yday = origin_components.yday;
  bool origin_leap = is_leap_year(origin_year_offset + 1970);

  for (R_xlen_t i = 0; i < size; ++i) {
    int elt = p_x[i];

    if (elt == NA_INTEGER) {
      p_out[i] = NA_REAL;
      continue;
    }

    struct warp_components components = convert_days_to_components(elt);

    p_out[i] = compute_yday_distance(
      elt,
      components.year_offset,
      components.yday,
      origin_year_offset,
      origin_yday,
      origin_leap,
      units_in_leap_year,
      units_in_non_leap_year,
      every
    );
  }

  UNPROTECT(1);
  return out;
}

static SEXP dbl_date_warp_distance_yday(SEXP x, int every, SEXP origin) {
  double* p_x = REAL(x);

  R_xlen_t size = Rf_xlength(x);

  SEXP out = PROTECT(Rf_allocVector(REALSXP, size));
  double* p_out = REAL(out);

  int units_in_non_leap_year = (DAYS_IN_YEAR - 1) / every + 1;
  int units_in_leap_year = (DAYS_IN_LEAP_YEAR - 1) / every + 1;

  struct warp_yday_components origin_components = get_origin_yday_components(origin);
  int origin_year_offset = origin_components.year_offset;
  int origin_yday = origin_components.yday;
  bool origin_leap = is_leap_year(origin_year_offset + 1970);

  for (R_xlen_t i = 0; i < size; ++i) {
    double x_elt = p_x[i];

    if (!R_FINITE(x_elt)) {
      p_out[i] = NA_REAL;
      continue;
    }

    // Truncate fractional pieces towards 0
    int elt = x_elt;

    struct warp_components components = convert_days_to_components(elt);

    p_out[i] = compute_yday_distance(
      elt,
      components.year_offset,
      components.yday,
      origin_year_offset,
      origin_yday,
      origin_leap,
      units_in_leap_year,
      units_in_non_leap_year,
      every
    );
  }

  UNPROTECT(1);
  return out;
}

#undef DAYS_IN_YEAR
#undef DAYS_IN_LEAP_YEAR

static inline int leap_years_before_and_including_year(int year_offset);
static inline int yday_leap_adjustment(int year_offset, int yday, bool origin_leap);

static int compute_yday_distance(int days_since_epoch,
                                 int year_offset,
                                 int yday,
                                 int origin_year_offset,
                                 int origin_yday,
                                 int origin_leap,
                                 int units_in_leap_year,
                                 int units_in_non_leap_year,
                                 int every) {
  int origin_yday_adjusted =
    origin_yday +
    yday_leap_adjustment(year_offset, yday, origin_leap);

  int last_origin_year_offset = year_offset;
  if (yday < origin_yday_adjusted) {
    --last_origin_year_offset;
  }

  int last_origin =
    days_before_year(last_origin_year_offset) +
    origin_yday +
    yday_leap_adjustment(last_origin_year_offset, origin_yday, origin_leap);

  int days_since_last_origin = days_since_epoch - last_origin;

  int units_in_year = int_div(days_since_last_origin, every);

  int years_between_origins = last_origin_year_offset - origin_year_offset;

  int leap_years_between_origins =
    leap_years_before_and_including_year(last_origin_year_offset) -
    leap_years_before_and_including_year(origin_year_offset);

  int non_leap_years_between_origins =
    years_between_origins -
    leap_years_between_origins;

  int units_between_origins =
    units_in_leap_year * leap_years_between_origins +
    units_in_non_leap_year * non_leap_years_between_origins;

  int out = units_between_origins + units_in_year;

  return out;
}

// Returns the number of days between 1970-01-01 and the beginning of the `year`
// defined as the number of `year_offset` from 1970, 0-based
// Not 1969 because `year_offset` is 0 based already
#define YEARS_FROM_0001_01_01_TO_EPOCH 1970
#define DAYS_FROM_0001_01_01_TO_EPOCH 719162
#define LEAP_YEARS_FROM_0001_01_01_TO_EPOCH 477

static inline int days_before_year(int year_offset) {
  int year = year_offset + YEARS_FROM_0001_01_01_TO_EPOCH;
  --year;

  int days = year * 365 +
    int_div(year, 4) -
    int_div(year, 100) +
    int_div(year, 400);

  days -= DAYS_FROM_0001_01_01_TO_EPOCH;

  return days;
}

static inline int leap_years_before_and_including_year(int year_offset) {
  int year = year_offset + YEARS_FROM_0001_01_01_TO_EPOCH;

  int n_leap_years =
    int_div(year, 4) -
    int_div(year, 100) +
    int_div(year, 400);

  n_leap_years -= LEAP_YEARS_FROM_0001_01_01_TO_EPOCH;

  return n_leap_years;
}

#undef YEARS_FROM_0001_01_01_TO_EPOCH
#undef DAYS_FROM_0001_01_01_TO_EPOCH
#undef LEAP_YEARS_FROM_0001_01_01_TO_EPOCH

static inline int yday_leap_adjustment(int year_offset, int yday, bool origin_leap) {
  // No adjustment to make if before or equal to Feb 28th
  if (yday < 58) {
    return 0;
  }

  int year = year_offset + 1970;

  bool year_is_leap = is_leap_year(year);

  if (origin_leap) {
    if (year_is_leap) {
      return 0;
    } else {
      return -1;
    }
  } else {
    if (year_is_leap) {
      return 1;
    } else {
      return 0;
    }
  }
}

#undef is_leap_year

// -----------------------------------------------------------------------------

static SEXP date_warp_distance_hour(SEXP x, int every, SEXP origin);
static SEXP posixct_warp_distance_hour(SEXP x, int every, SEXP origin);
static SEXP posixlt_warp_distance_hour(SEXP x, int every, SEXP origin);

static SEXP warp_distance_hour(SEXP x, int every, SEXP origin) {
  switch (time_class_type(x)) {
  case warp_class_date: return date_warp_distance_hour(x, every, origin);
  case warp_class_posixct: return posixct_warp_distance_hour(x, every, origin);
  case warp_class_posixlt: return posixlt_warp_distance_hour(x, every, origin);
  default: r_error("warp_distance_hour", "Unknown object with type, %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_date_warp_distance_hour(SEXP x, int every, SEXP origin);
static SEXP dbl_date_warp_distance_hour(SEXP x, int every, SEXP origin);

static SEXP date_warp_distance_hour(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_date_warp_distance_hour(x, every, origin);
  case REALSXP: return dbl_date_warp_distance_hour(x, every, origin);
  default: r_error("date_warp_distance_hour", "Unknown `Date` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_posixct_warp_distance_hour(SEXP x, int every, SEXP origin);
static SEXP dbl_posixct_warp_distance_hour(SEXP x, int every, SEXP origin);

static SEXP posixct_warp_distance_hour(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_posixct_warp_distance_hour(x, every, origin);
  case REALSXP: return dbl_posixct_warp_distance_hour(x, every, origin);
  default: r_error("posixct_warp_distance_hour", "Unknown `POSIXct` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP posixlt_warp_distance_hour(SEXP x, int every, SEXP origin) {
  x = PROTECT(as_datetime(x));
  SEXP out = PROTECT(posixct_warp_distance_hour(x, every, origin));

  UNPROTECT(2);
  return out;
}


#define HOURS_IN_DAY 24

static SEXP int_date_warp_distance_hour(SEXP x, int every, SEXP origin) {
  R_xlen_t size = Rf_xlength(x);

  int* p_x = INTEGER(x);

  SEXP out = PROTECT(Rf_allocVector(REALSXP, size));
  double* p_out = REAL(out);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  int origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_days_from_epoch(origin);
  }

  for (R_xlen_t i = 0; i < size; ++i) {
    int elt = p_x[i];

    if (elt == NA_INTEGER) {
      p_out[i] = NA_REAL;
      continue;
    }

    if (needs_offset) {
      elt -= origin_offset;
    }

    elt *= HOURS_IN_DAY;

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

static SEXP dbl_date_warp_distance_hour(SEXP x, int every, SEXP origin) {
  R_xlen_t size = Rf_xlength(x);

  double* p_x = REAL(x);

  SEXP out = PROTECT(Rf_allocVector(REALSXP, size));
  double* p_out = REAL(out);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  int origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_days_from_epoch(origin);
  }

  for (R_xlen_t i = 0; i < size; ++i) {
    double x_elt = p_x[i];

    if (!R_FINITE(x_elt)) {
      p_out[i] = NA_REAL;
      continue;
    }

    // Truncate to completely ignore fractional Date parts
    int elt = x_elt;

    if (needs_offset) {
      elt -= origin_offset;
    }

    elt *= HOURS_IN_DAY;

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

static SEXP int_posixct_warp_distance_hour(SEXP x, int every, SEXP origin) {
  R_xlen_t size = Rf_xlength(x);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  int64_t origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_seconds_from_epoch(origin);
  }

  SEXP out = PROTECT(Rf_allocVector(REALSXP, size));
  double* p_out = REAL(out);

  int* p_x = INTEGER(x);

  for (R_xlen_t i = 0; i < size; ++i) {
    int x_elt = p_x[i];

    if (x_elt == NA_INTEGER) {
      p_out[i] = NA_REAL;
      continue;
    }

    // Avoid overflow
    int64_t elt = x_elt;

    if (needs_offset) {
      elt -= origin_offset;
    }

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

static SEXP dbl_posixct_warp_distance_hour(SEXP x, int every, SEXP origin) {
  R_xlen_t size = Rf_xlength(x);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  int64_t origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_seconds_from_epoch(origin);
  }

  SEXP out = PROTECT(Rf_allocVector(REALSXP, size));
  double* p_out = REAL(out);

  double* p_x = REAL(x);

  for (R_xlen_t i = 0; i < size; ++i) {
    double x_elt = p_x[i];

    if (!R_FINITE(x_elt)) {
      p_out[i] = NA_REAL;
      continue;
    }

    int64_t elt = guarded_floor(x_elt);

    if (needs_offset) {
      elt -= origin_offset;
    }

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

#undef SECONDS_IN_HOUR

// -----------------------------------------------------------------------------

static SEXP date_warp_distance_minute(SEXP x, int every, SEXP origin);
static SEXP posixct_warp_distance_minute(SEXP x, int every, SEXP origin);
static SEXP posixlt_warp_distance_minute(SEXP x, int every, SEXP origin);

static SEXP warp_distance_minute(SEXP x, int every, SEXP origin) {
  switch (time_class_type(x)) {
  case warp_class_date: return date_warp_distance_minute(x, every, origin);
  case warp_class_posixct: return posixct_warp_distance_minute(x, every, origin);
  case warp_class_posixlt: return posixlt_warp_distance_minute(x, every, origin);
  default: r_error("warp_distance_minute", "Unknown object with type, %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_date_warp_distance_minute(SEXP x, int every, SEXP origin);
static SEXP dbl_date_warp_distance_minute(SEXP x, int every, SEXP origin);

static SEXP date_warp_distance_minute(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_date_warp_distance_minute(x, every, origin);
  case REALSXP: return dbl_date_warp_distance_minute(x, every, origin);
  default: r_error("date_warp_distance_minute", "Unknown `Date` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_posixct_warp_distance_minute(SEXP x, int every, SEXP origin);
static SEXP dbl_posixct_warp_distance_minute(SEXP x, int every, SEXP origin);

static SEXP posixct_warp_distance_minute(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_posixct_warp_distance_minute(x, every, origin);
  case REALSXP: return dbl_posixct_warp_distance_minute(x, every, origin);
  default: r_error("posixct_warp_distance_minute", "Unknown `POSIXct` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP posixlt_warp_distance_minute(SEXP x, int every, SEXP origin) {
  x = PROTECT(as_datetime(x));
  SEXP out = PROTECT(posixct_warp_distance_minute(x, every, origin));

  UNPROTECT(2);
  return out;
}


#define MINUTES_IN_DAY 1440

static SEXP int_date_warp_distance_minute(SEXP x, int every, SEXP origin) {
  R_xlen_t size = Rf_xlength(x);

  int* p_x = INTEGER(x);

  SEXP out = PROTECT(Rf_allocVector(REALSXP, size));
  double* p_out = REAL(out);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  int origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_days_from_epoch(origin);
  }

  for (R_xlen_t i = 0; i < size; ++i) {
    int elt = p_x[i];

    if (elt == NA_INTEGER) {
      p_out[i] = NA_REAL;
      continue;
    }

    if (needs_offset) {
      elt -= origin_offset;
    }

    elt *= MINUTES_IN_DAY;

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

static SEXP dbl_date_warp_distance_minute(SEXP x, int every, SEXP origin) {
  R_xlen_t size = Rf_xlength(x);

  double* p_x = REAL(x);

  SEXP out = PROTECT(Rf_allocVector(REALSXP, size));
  double* p_out = REAL(out);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  int origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_days_from_epoch(origin);
  }

  for (R_xlen_t i = 0; i < size; ++i) {
    double x_elt = p_x[i];

    if (!R_FINITE(x_elt)) {
      p_out[i] = NA_REAL;
      continue;
    }

    // Truncate to completely ignore fractional Date parts
    int elt = x_elt;

    if (needs_offset) {
      elt -= origin_offset;
    }

    elt *= MINUTES_IN_DAY;

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

static SEXP int_posixct_warp_distance_minute(SEXP x, int every, SEXP origin) {
  R_xlen_t size = Rf_xlength(x);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  int64_t origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_seconds_from_epoch(origin);
  }

  SEXP out = PROTECT(Rf_allocVector(REALSXP, size));
  double* p_out = REAL(out);

  int* p_x = INTEGER(x);

  for (R_xlen_t i = 0; i < size; ++i) {
    int x_elt = p_x[i];

    if (x_elt == NA_INTEGER) {
      p_out[i] = NA_REAL;
      continue;
    }

    // Avoid overflow
    int64_t elt = x_elt;

    if (needs_offset) {
      elt -= origin_offset;
    }

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

static SEXP dbl_posixct_warp_distance_minute(SEXP x, int every, SEXP origin) {
  R_xlen_t size = Rf_xlength(x);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  int64_t origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_seconds_from_epoch(origin);
  }

  SEXP out = PROTECT(Rf_allocVector(REALSXP, size));
  double* p_out = REAL(out);

  double* p_x = REAL(x);

  for (R_xlen_t i = 0; i < size; ++i) {
    double x_elt = p_x[i];

    if (!R_FINITE(x_elt)) {
      p_out[i] = NA_REAL;
      continue;
    }

    int64_t elt = guarded_floor(x_elt);

    if (needs_offset) {
      elt -= origin_offset;
    }

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

#undef SECONDS_IN_MINUTE

// -----------------------------------------------------------------------------

static SEXP date_warp_distance_second(SEXP x, int every, SEXP origin);
static SEXP posixct_warp_distance_second(SEXP x, int every, SEXP origin);
static SEXP posixlt_warp_distance_second(SEXP x, int every, SEXP origin);

static SEXP warp_distance_second(SEXP x, int every, SEXP origin) {
  switch (time_class_type(x)) {
  case warp_class_date: return date_warp_distance_second(x, every, origin);
  case warp_class_posixct: return posixct_warp_distance_second(x, every, origin);
  case warp_class_posixlt: return posixlt_warp_distance_second(x, every, origin);
  default: r_error("warp_distance_second", "Unknown object with type, %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_date_warp_distance_second(SEXP x, int every, SEXP origin);
static SEXP dbl_date_warp_distance_second(SEXP x, int every, SEXP origin);

static SEXP date_warp_distance_second(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_date_warp_distance_second(x, every, origin);
  case REALSXP: return dbl_date_warp_distance_second(x, every, origin);
  default: r_error("date_warp_distance_second", "Unknown `Date` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_posixct_warp_distance_second(SEXP x, int every, SEXP origin);
static SEXP dbl_posixct_warp_distance_second(SEXP x, int every, SEXP origin);

static SEXP posixct_warp_distance_second(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_posixct_warp_distance_second(x, every, origin);
  case REALSXP: return dbl_posixct_warp_distance_second(x, every, origin);
  default: r_error("posixct_warp_distance_second", "Unknown `POSIXct` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP posixlt_warp_distance_second(SEXP x, int every, SEXP origin) {
  x = PROTECT(as_datetime(x));
  SEXP out = PROTECT(posixct_warp_distance_second(x, every, origin));

  UNPROTECT(2);
  return out;
}


#define SECONDS_IN_DAY 86400

static SEXP int_date_warp_distance_second(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  int* p_x = INTEGER(x);

  SEXP out = PROTECT(Rf_allocVector(REALSXP, x_size));
  double* p_out = REAL(out);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  int origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_days_from_epoch(origin);
  }

  for (R_xlen_t i = 0; i < x_size; ++i) {
    int x_elt = p_x[i];

    if (x_elt == NA_INTEGER) {
      p_out[i] = NA_REAL;
      continue;
    }

    // Avoid overflow
    int64_t elt = x_elt;

    if (needs_offset) {
      elt -= origin_offset;
    }

    elt *= SECONDS_IN_DAY;

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

static SEXP dbl_date_warp_distance_second(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  double* p_x = REAL(x);

  SEXP out = PROTECT(Rf_allocVector(REALSXP, x_size));
  double* p_out = REAL(out);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  int origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_days_from_epoch(origin);
  }

  for (R_xlen_t i = 0; i < x_size; ++i) {
    double x_elt = p_x[i];

    if (!R_FINITE(x_elt)) {
      p_out[i] = NA_REAL;
      continue;
    }

    // Truncate to completely ignore fractional Date parts
    // `int64_t` to avoid overflow
    int64_t elt = x_elt;

    if (needs_offset) {
      elt -= origin_offset;
    }

    elt *= SECONDS_IN_DAY;

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

static SEXP int_posixct_warp_distance_second(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  int64_t origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_seconds_from_epoch(origin);
  }

  SEXP out = PROTECT(Rf_allocVector(REALSXP, x_size));
  double* p_out = REAL(out);

  int* p_x = INTEGER(x);

  for (R_xlen_t i = 0; i < x_size; ++i) {
    int x_elt = p_x[i];

    if (x_elt == NA_INTEGER) {
      p_out[i] = NA_REAL;
      continue;
    }

    // Avoid overflow
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

static SEXP dbl_posixct_warp_distance_second(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  int64_t origin_offset;

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

    int64_t elt = guarded_floor(x_elt);

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

// -----------------------------------------------------------------------------

static SEXP date_warp_distance_millisecond(SEXP x, int every, SEXP origin);
static SEXP posixct_warp_distance_millisecond(SEXP x, int every, SEXP origin);
static SEXP posixlt_warp_distance_millisecond(SEXP x, int every, SEXP origin);

static SEXP warp_distance_millisecond(SEXP x, int every, SEXP origin) {
  switch (time_class_type(x)) {
  case warp_class_date: return date_warp_distance_millisecond(x, every, origin);
  case warp_class_posixct: return posixct_warp_distance_millisecond(x, every, origin);
  case warp_class_posixlt: return posixlt_warp_distance_millisecond(x, every, origin);
  default: r_error("warp_distance_millisecond", "Unknown object with type, %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_date_warp_distance_millisecond(SEXP x, int every, SEXP origin);
static SEXP dbl_date_warp_distance_millisecond(SEXP x, int every, SEXP origin);

static SEXP date_warp_distance_millisecond(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_date_warp_distance_millisecond(x, every, origin);
  case REALSXP: return dbl_date_warp_distance_millisecond(x, every, origin);
  default: r_error("date_warp_distance_millisecond", "Unknown `Date` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP int_posixct_warp_distance_millisecond(SEXP x, int every, SEXP origin);
static SEXP dbl_posixct_warp_distance_millisecond(SEXP x, int every, SEXP origin);

static SEXP posixct_warp_distance_millisecond(SEXP x, int every, SEXP origin) {
  switch (TYPEOF(x)) {
  case INTSXP: return int_posixct_warp_distance_millisecond(x, every, origin);
  case REALSXP: return dbl_posixct_warp_distance_millisecond(x, every, origin);
  default: r_error("posixct_warp_distance_millisecond", "Unknown `POSIXct` type %s.", Rf_type2char(TYPEOF(x)));
  }
}


static SEXP posixlt_warp_distance_millisecond(SEXP x, int every, SEXP origin) {
  x = PROTECT(as_datetime(x));
  SEXP out = PROTECT(posixct_warp_distance_millisecond(x, every, origin));

  UNPROTECT(2);
  return out;
}


#define MILLISECONDS_IN_DAY 86400000

static SEXP int_date_warp_distance_millisecond(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  int* p_x = INTEGER(x);

  SEXP out = PROTECT(Rf_allocVector(REALSXP, x_size));
  double* p_out = REAL(out);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  int origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_days_from_epoch(origin);
  }

  for (R_xlen_t i = 0; i < x_size; ++i) {
    int x_elt = p_x[i];

    if (x_elt == NA_INTEGER) {
      p_out[i] = NA_REAL;
      continue;
    }

    // `int64_t` to avoid overflow
    int64_t elt = x_elt;

    if (needs_offset) {
      elt -= origin_offset;
    }

    elt *= MILLISECONDS_IN_DAY;

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

static SEXP dbl_date_warp_distance_millisecond(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  double* p_x = REAL(x);

  SEXP out = PROTECT(Rf_allocVector(REALSXP, x_size));
  double* p_out = REAL(out);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  int origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_days_from_epoch(origin);
  }

  for (R_xlen_t i = 0; i < x_size; ++i) {
    double x_elt = p_x[i];

    if (!R_FINITE(x_elt)) {
      p_out[i] = NA_REAL;
      continue;
    }

    // Truncate to completely ignore fractional Date parts
    // `int64_t` to avoid overflow
    int64_t elt = x_elt;

    if (needs_offset) {
      elt -= origin_offset;
    }

    elt *= MILLISECONDS_IN_DAY;

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

#undef MILLISECONDS_IN_DAY

#define MILLISECONDS_IN_SECOND 1000

static SEXP int_posixct_warp_distance_millisecond(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  int64_t origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_milliseconds_from_epoch(origin);
  }

  SEXP out = PROTECT(Rf_allocVector(REALSXP, x_size));
  double* p_out = REAL(out);

  int* p_x = INTEGER(x);

  for (R_xlen_t i = 0; i < x_size; ++i) {
    int x_elt = p_x[i];

    if (x_elt == NA_INTEGER) {
      p_out[i] = NA_REAL;
      continue;
    }

    // `int64_t` to avoid overflow
    // Note - Have to do `* MILLISECONDS_IN_SECOND` before the
    // offset subtraction because the offset is already in milliseconds
    int64_t elt = x_elt * MILLISECONDS_IN_SECOND;

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

static SEXP dbl_posixct_warp_distance_millisecond(SEXP x, int every, SEXP origin) {
  R_xlen_t x_size = Rf_xlength(x);

  bool needs_every = (every != 1);

  bool needs_offset = (origin != R_NilValue);
  int64_t origin_offset;

  if (needs_offset) {
    origin_offset = origin_to_milliseconds_from_epoch(origin);
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

    int64_t elt = guarded_floor_to_millisecond(x_elt);

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

#undef MILLISECONDS_IN_SECOND

// -----------------------------------------------------------------------------

static void validate_every(int every) {
  if (every == NA_INTEGER) {
    r_error("validate_every", "`every` must not be `NA`");
  }

  if (every <= 0) {
    r_error("validate_every", "`every` must be an integer greater than 0, not %i", every);
  }
}

static void validate_origin(SEXP origin) {
  if (origin == R_NilValue) {
    return;
  }

  R_len_t n_origin = Rf_length(origin);

  if (n_origin != 1) {
    r_error("validate_origin", "`origin` must have size 1, not %i.", n_origin);
  }

  if (time_class_type(origin) == warp_class_unknown) {
    r_error("validate_origin", "`origin` must inherit from 'Date', 'POSIXct', or 'POSIXlt'.");
  }
}

// `as_date()` will always return a double with no fractional component,
// and the double will always fit inside an int
static int origin_to_days_from_epoch(SEXP origin) {
  origin = PROTECT(as_date(origin));

  double out = REAL(origin)[0];

  if (out == NA_REAL) {
    r_error("origin_to_days_from_epoch", "`origin` must not be `NA`.");
  }

  UNPROTECT(1);
  return (int) out;
}

static int64_t origin_to_seconds_from_epoch(SEXP origin) {
  origin = PROTECT(as_datetime(origin));

  double origin_value = REAL(origin)[0];

  if (origin_value == NA_REAL) {
    r_error("origin_to_seconds_from_epoch", "`origin` must not be `NA`.");
  }

  int64_t out = guarded_floor(origin_value);

  UNPROTECT(1);
  return out;
}

static int64_t origin_to_milliseconds_from_epoch(SEXP origin) {
  origin = PROTECT(as_datetime(origin));

  double origin_value = REAL(origin)[0];

  if (origin_value == NA_REAL) {
    r_error("origin_to_milliseconds_from_epoch", "`origin` must not be `NA`.");
  }

  int64_t out = guarded_floor_to_millisecond(origin_value);

  UNPROTECT(1);
  return out;
}

/*
 * `double` values are represented with 64 bits:
 * - 1 sign bit
 * - 11 exponent bits
 * - 52 significand bits
 *
 * The 52 significand bits are the ones that store the true value, this
 * corresponds to about ~16 significand digits, with everything after
 * that being garbage.
 *
 * Internally doubles are represented with scientific notation to put them in
 * the exponent-significand representation. So the following date, which
 * is represented as a double, really looks like this in scientific notation:
 *
 * unclass(as.POSIXct("2011-05-01 17:55:23.123456"))
 * =
 * 1304286923.1234560013
 * =
 * 1.3042869231234560013e+09
 *                 ^ 16th digit
 *
 * Because only ~16 digits are stable, this is where we draw the line on
 * assuming that the user might have some valuable information stored here.
 * This corresponds to microseconds. Sure, we could use
 * a date that has less digits before the decimal to get more fractional
 * precision (see below) but most dates are in this form: 10 digits before
 * the decimal representing whole seconds, meaning 6 stable digits after it.
 *
 * The other part of the story is that not all floating point numbers can be
 * represented exactly in binary. For example:
 *
 * unclass(as.POSIXct("1969-12-31 23:59:59.998", "UTC"))
 * =
 * -0.002000000000002444267
 *
 * Because of this, `floor()` will give results that (to us) are incorrect if
 * we were to try and floor to milliseconds. We would first times by 1000 to
 * get milliseconds of `-2.000000000002444267`, and then `floor()` would give
 * us -3, not -2 which is the correct group.
 *
 * To get around this, we need to guard against this floating point error. The
 * best way I can come up with is to add a small value before flooring, which
 * would push us into the -1.9999999 range, which would floor correctly.
 *
 * I chose the value of just beyond 1 microsecond because that is generally
 * where the 17th digit falls for most dates
 * (10 digits of whole seconds, 5 of stable fractional seconds). This seems to
 * work well for the millisecond grouping, and we apply it to anywhere that
 * uses seconds "just in case", but it is hard to come up with tests for them.
 */

static inline int64_t guarded_floor(double x) {
  // Scale and trim past microseconds
  x *= 1e6;
  x = trunc(x);
  x *= 1e-6;

  // Add guard and floor
  x += 1e-7;
  x = floor(x);

  return (int64_t) x;
}

// The order here is slightly different. We want to convert
// seconds to milliseconds while still guarding correctly.
// - Scale and trim past microseconds
// - Guard while still at the second level to put it on the right decimal
// - Now scale to millisecond and floor

static inline int64_t guarded_floor_to_millisecond(double x) {
  // Scale and trim past microseconds
  x *= 1e6;
  x = trunc(x);
  x *= 1e-6;

  // Add guard, scale to milliseconds, and floor
  x += 1e-7;
  x *= 1e3;
  x = floor(x);

  return (int64_t) x;
}
