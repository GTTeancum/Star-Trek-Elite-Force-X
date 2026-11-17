#ifndef STEFX_SPLIT_PERF_DEFAULTS_H
#define STEFX_SPLIT_PERF_DEFAULTS_H

// Build-time selection after console qualification. Every implementation and
// fallback is compiled before testing; selecting a measured policy needs no
// later source edit. Ordinary builds retain opt-in candidate defaults.
#ifndef STEFX_SPLIT_PERF_DEFAULTS
#define STEFX_SPLIT_PERF_DEFAULTS 0
#endif
#ifndef STEFX_HM_WORLD_DEFAULT_MODE
#define STEFX_HM_WORLD_DEFAULT_MODE 2
#endif
#if STEFX_SPLIT_PERF_DEFAULTS < 0 || STEFX_SPLIT_PERF_DEFAULTS > 15
#error Invalid split performance defaults mask
#endif
#if STEFX_HM_WORLD_DEFAULT_MODE < 0 || STEFX_HM_WORLD_DEFAULT_MODE > 2
#error Invalid Holomatch world mode
#endif
#if STEFX_SPLIT_PERF_DEFAULTS & 1
#define STEFX_SPLIT_FX_DEFAULT "1"
#else
#define STEFX_SPLIT_FX_DEFAULT "0"
#endif
#if STEFX_SPLIT_PERF_DEFAULTS & 2
#define STEFX_SPLIT_MODEL_DEFAULT "1"
#else
#define STEFX_SPLIT_MODEL_DEFAULT "0"
#endif
#if STEFX_SPLIT_PERF_DEFAULTS & 4
#define STEFX_SPLIT_VIS_DEFAULT "1"
#else
#define STEFX_SPLIT_VIS_DEFAULT "0"
#endif
#if STEFX_SPLIT_PERF_DEFAULTS & 8
#define STEFX_SPLIT_LIGHT_DEFAULT "1"
#else
#define STEFX_SPLIT_LIGHT_DEFAULT "0"
#endif
#define STEFX_SPLIT_STRING_INNER(value) #value
#define STEFX_SPLIT_STRING(value) STEFX_SPLIT_STRING_INNER(value)
#define STEFX_HM_WORLD_DEFAULT STEFX_SPLIT_STRING(STEFX_HM_WORLD_DEFAULT_MODE)
#endif
