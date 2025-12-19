#ifndef JSON_HELPER_H
#define JSON_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

int json_helper_update_in_data(const char* json_str);
int json_helper_update_out_data(const char* json_str);
int json_helper_update_license(const char* json_str);
int json_helper_update_out_sw(const char* json_str);
int json_helper_update_modbus(const char* json_str);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* JSON_HELPER_H */
