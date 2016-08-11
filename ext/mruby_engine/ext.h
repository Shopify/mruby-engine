#ifndef MRUBY_ENGINE_EXT_H
#define MRUBY_ENGINE_EXT_H

#include <ruby.h>

extern ID me_ext_id_guest_backtrace_eq;
extern ID me_ext_id_generate;
extern ID me_ext_id_instructions;
extern ID me_ext_id_memory;
extern ID me_ext_id_type_eq;
extern VALUE me_ext_m_json;
extern VALUE me_ext_c_mruby_engine;
extern VALUE me_ext_c_iseq;
extern VALUE me_ext_e_engine_error;
extern VALUE me_ext_e_engine_runtime_error;
extern VALUE me_ext_e_engine_type_error;
extern VALUE me_ext_e_engine_syntax_error;
extern VALUE me_ext_e_engine_quota_error;
extern VALUE me_ext_e_engine_memory_quota_error;
extern VALUE me_ext_e_engine_instruction_quota_error;
extern VALUE me_ext_e_engine_time_quota_error;
extern VALUE me_ext_e_engine_stack_exhausted_error;
extern VALUE me_ext_e_engine_internal_error;
extern VALUE me_ext_e_engine_quota_already_reached;

#endif
