/* forward_decls.h — Forward declarations for source code ordering issues
   Used via -include for uwb_ncj29d6.c only.
   The type distance_zone_e is defined in iccoa_vehicle_types.h or ccc_digital_key.h
   which are already available via include paths. Only the function decl is needed. */
static distance_zone_e classify_distance_impl(uint16_t dist_cm);
