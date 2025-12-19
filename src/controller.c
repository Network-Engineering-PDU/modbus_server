#include <cjson/cJSON.h>
#include <stdio.h>
#include <strings.h>

#include "controller.h"
#include "models.h"

#include "http_helper.h"
#include "json_helper.h"

#define BASE_URL "http://localhost:8001/"

/* Global variables ***********************************************************/
/* Function prototypes ********************************************************/
/* Callbacks ******************************************************************/
/* Function definitions *******************************************************/
/* Public functions ***********************************************************/

void controller_init()
{
}

void controller_get_in_data(int line_id)
{
	http_get_req_t req;
	char url[100];
	sprintf(url, BASE_URL "inputs/%d/data", line_id);
	http_helper_get(&req, url);
	json_helper_update_in_data(req.buffer);
	http_helper_free(&req);
}

void controller_get_out_data(int line_id)
{
	http_get_req_t req;
	char url[100];
	sprintf(url, BASE_URL "outputs/%d/data", line_id);
	http_helper_get(&req, url);
	json_helper_update_out_data(req.buffer);
	http_helper_free(&req);
}

void controller_get_license()
{
	http_get_req_t req;
	char* url = BASE_URL "settings/license";
	http_helper_get(&req, url);
	json_helper_update_license(req.buffer);
	http_helper_free(&req);
}

void controller_get_out_sw()
{
	http_get_req_t req;
	char* url = BASE_URL "outputs/switch-status/";
	http_helper_get(&req, url);
	json_helper_update_out_sw(req.buffer);
	http_helper_free(&req);
}

void controller_put_out_sw(const models_out_sw_t* out_sw, int line_id)
{
	http_get_req_t req;
	char url[100];
	sprintf(url, BASE_URL "outputs/%d/switch-status", line_id);
	cJSON *json = cJSON_CreateObject();
	cJSON_AddBoolToObject(json, "switch_status", out_sw->status);
	char* put_data = cJSON_PrintUnformatted(json);
	int err = http_helper_put(&req, url, put_data);
	if (err == 0) {
		models_set_out_sw_idx(out_sw, line_id);
	}
	cJSON_Delete(json);
	http_helper_free(&req);
}

void controller_get_modbus()
{
	http_get_req_t req;
	char* url = BASE_URL "settings/modbus";
	http_helper_get(&req, url);
	json_helper_update_modbus(req.buffer);
	http_helper_free(&req);
}